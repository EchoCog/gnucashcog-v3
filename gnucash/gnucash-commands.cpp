/*
 * gnucash-cli.cpp -- The command line entry point for GnuCash
 *
 * Copyright (C) 2020 Geert Janssens <geert@kobaltwit.be>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, contact:
 *
 * Free Software Foundation           Voice:  +1-617-542-5942
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652
 * Boston, MA  02110-1301,  USA       gnu@gnu.org
 */
#include <config.h>

#include <libguile.h>
#include <guile-mappings.h>
#ifdef __MINGW32__
#include <Windows.h>
#include <fcntl.h>
#endif

#include "gnucash-commands.hpp"
#include "gnucash-core-app.hpp"

#include <gnc-filepath-utils.h>
#include <gnc-engine-guile.h>
#include <gnc-prefs.h>
#include <gnc-prefs-utils.h>
#include <gnc-session.h>
#include <qoflog.h>
#include <gnc-fincosys-bridge.h>
#include <gnc-cognitive-accounting.h>
#include <gnc-cognitive-fincosys-loader.h>
#include <Account.h>
#include <Transaction.h>
#include <Split.h>
#include <engine-helpers.h>

#include <boost/locale.hpp>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <gnc-report.h>
#include <gnc-quotes.hpp>

namespace bl = boost::locale;

static std::string empty_string{};

/* This static indicates the debugging module that this .o belongs to.  */
static QofLogModule log_module = GNC_MOD_GUI;

static int
cleanup_and_exit_with_failure (QofSession *session)
{
    if (session)
    {
        auto error{qof_session_get_error (session)};
        if (error != ERR_BACKEND_NO_ERR)
        {
            if (error == ERR_BACKEND_LOCKED)
                PERR ("File is locked, won't open.");
            else
                PERR ("Session Error: %s\n",
                      qof_session_get_error_message (session));
        }
        qof_session_destroy (session);
    }
    qof_event_resume();
    return 1;
}

static void gnc_shutdown_cli (int exit_status)
{
    gnc_hook_run (HOOK_SHUTDOWN, NULL);
    gnc_engine_shutdown ();
    exit (exit_status);
}

/* scm_boot_guile doesn't expect to return, so call shutdown ourselves here */
static void
scm_cleanup_and_exit_with_failure (QofSession *session)
{
    cleanup_and_exit_with_failure (session);
    gnc_shutdown_cli (1);
}

static void
report_session_percentage (const char *message, double percent)
{
    static double previous = 0.0;
    if ((percent - previous) < 5.0)
        return;
    PINFO ("\r%3.0f%% complete...", percent);
    previous = percent;
    return;
}

/* Don't try to use std::string& for the members of the following struct, it
 * results in the values getting corrupted as it passes through initializing
 * Scheme when compiled with Clang.
 */
struct run_report_args {
    const std::string& file_to_load;
    const std::string& run_report;
    const std::string& export_type;
    const std::string& output_file;
};

static inline void
write_report_file (const char *html, const char* file)
{
    if (!file || !html || !*html) return;
    auto ofs{gnc_open_filestream(file)};
    if (!ofs)
    {
        std::cerr << "Failed to open file " << file << " for writing\n";
        return;
    }
    ofs << html << std::endl;
    // ofs destructor will close the file
}

static void
scm_run_report (void *data,
                [[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
    auto args = static_cast<run_report_args*>(data);

    scm_c_eval_string("(debug-set! stack 200000)");
    scm_c_use_module ("gnucash utilities");
    scm_c_use_module ("gnucash app-utils");
    scm_c_use_module ("gnucash reports");

    gnc_report_init ();
    Gnucash::gnc_load_scm_config ([](const gchar *msg){ PINFO ("%s", msg); });
    gnc_prefs_init ();
    qof_event_suspend ();

    auto datafile = args->file_to_load.c_str();
    auto check_report_cmd = scm_c_eval_string ("gnc:cmdline-check-report");
    auto get_report_cmd = scm_c_eval_string ("gnc:cmdline-get-report-id");
    auto run_export_cmd = scm_c_eval_string ("gnc:cmdline-template-export");
    /* We generally insist on using scm_from_utf8_string() throughout GnuCash
     * because all GUI-sourced strings and all file-sourced strings are encoded
     * that way. In this case, though, the input is coming from a shell window
     * and Microsoft Windows shells are generally not capable of entering UTF8
     * so it's necessary here to allow guile to read the locale and interpret
     * the input in that encoding.
     */
    auto report = scm_from_locale_string (args->run_report.c_str());
    auto type = !args->export_type.empty() ?
                scm_from_locale_string (args->export_type.c_str()) : SCM_BOOL_F;

    if (scm_is_false (scm_call_2 (check_report_cmd, report, type)))
        scm_cleanup_and_exit_with_failure (nullptr);

    PINFO ("Loading datafile %s...\n", datafile);

    auto session = gnc_get_current_session ();
    if (!session)
        scm_cleanup_and_exit_with_failure (session);

    qof_session_begin (session, datafile, SESSION_READ_ONLY);
    if (qof_session_get_error (session) != ERR_BACKEND_NO_ERR)
        scm_cleanup_and_exit_with_failure (session);

    qof_session_load (session, report_session_percentage);
    if (qof_session_get_error (session) != ERR_BACKEND_NO_ERR)
        scm_cleanup_and_exit_with_failure (session);

    if (!args->export_type.empty())
    {
        SCM retval = scm_call_2 (run_export_cmd, report, type);
        SCM query_result = scm_c_eval_string ("gnc:html-document?");
        SCM get_export_string = scm_c_eval_string ("gnc:html-document-export-string");
        SCM get_export_error = scm_c_eval_string ("gnc:html-document-export-error");

        if (scm_is_false (scm_call_1 (query_result, retval)))
        {
            std::cerr << _("This report must be upgraded to \
return a document object with export-string or export-error.") << std::endl;
            scm_cleanup_and_exit_with_failure (nullptr);
        }

        SCM export_string = scm_call_1 (get_export_string, retval);
        SCM export_error = scm_call_1 (get_export_error, retval);

        if (scm_is_string (export_string))
        {
            auto output = scm_to_utf8_string (export_string);
            if (!args->output_file.empty())
            {
                write_report_file(output, args->output_file.c_str());
            }
            else
            {
                std::cout << output << std::endl;
            }
            g_free (output);
        }
        else if (scm_is_string (export_error))
        {
            auto err = scm_to_utf8_string (export_error);
            std::cerr << err << std::endl;
            g_free (err);
            scm_cleanup_and_exit_with_failure (nullptr);
        }
        else
        {
            std::cerr << _("This report must be upgraded to \
return a document object with export-string or export-error.") << std::endl;
            scm_cleanup_and_exit_with_failure (nullptr);
        }
    }
    else
    {
        SCM id = scm_call_1(get_report_cmd, report);

        if (scm_is_false (id))
            scm_cleanup_and_exit_with_failure (nullptr);
        char *html, *errmsg;

        if (gnc_run_report_with_error_handling (scm_to_int(id), &html, &errmsg))
        {
            if (!args->output_file.empty())
            {
                write_report_file(html, args->output_file.c_str());
            }
            else
            {
                std::cout << html << std::endl;
            }
            g_free (html);
        }
        else
        {
            std::cerr << errmsg << std::endl;
            g_free (errmsg);
        }
    }

    qof_session_destroy (session);

    qof_event_resume ();
    gnc_shutdown_cli (0);
    return;
}


struct show_report_args {
    const std::string& file_to_load;
    const std::string& show_report;
};

static void
scm_report_show (void *data,
                [[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
    auto args = static_cast<show_report_args*>(data);

    scm_c_eval_string("(debug-set! stack 200000)");
    scm_c_use_module ("gnucash utilities");
    scm_c_use_module ("gnucash app-utils");
    scm_c_use_module ("gnucash reports");
    gnc_report_init ();
    Gnucash::gnc_load_scm_config ([](const gchar *msg){ PINFO ("%s", msg); });

    if (!args->file_to_load.empty())
    {
        auto datafile = args->file_to_load.c_str();
        PINFO ("Loading datafile %s...\n", datafile);

        auto session = gnc_get_current_session ();
        if (session)
        {
            qof_session_begin (session, datafile, SESSION_READ_ONLY);
            if (qof_session_get_error (session) == ERR_BACKEND_NO_ERR)
                qof_session_load (session, report_session_percentage);
        }
    }

    scm_call_2 (scm_c_eval_string ("gnc:cmdline-report-show"),
                scm_from_locale_string (args->show_report.c_str ()),
                scm_current_output_port ());
    gnc_shutdown_cli (0);
    return;
}


static void
scm_report_list ([[maybe_unused]] void *data,
                 [[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
    scm_c_eval_string("(debug-set! stack 200000)");
    scm_c_use_module ("gnucash app-utils");
    scm_c_use_module ("gnucash reports");
    gnc_report_init ();
    Gnucash::gnc_load_scm_config ([](const gchar *msg){ PINFO ("%s", msg); });

    scm_call_1 (scm_c_eval_string ("gnc:cmdline-report-list"),
                scm_current_output_port ());
    gnc_shutdown_cli (0);
    return;
}

int
Gnucash::check_finance_quote (void)
{
    gnc_prefs_init ();
    try
    {
        GncQuotes quotes;
        std::cout << bl::format (bl::translate ("Found Finance::Quote version {1}.")) % quotes.version() << "\n";
        std::cout << bl::translate ("Finance::Quote sources:\n");
        int count{0};
        const auto width{12};
        for (auto source : quotes.sources())
        {
            auto mul{source.length() / width + 1};
            count += mul;
            if (count > 6)
            {
                count = mul;
                std::cout << "\n";
            }
            std::cout << std::setw(mul * (width + 1)) << std::left << source;
        }
        std::cout << std::endl;
        return 0;
    }
    catch (const GncQuoteException& err)
    {
        std::cout << err.what() << std::endl;
        return 1;
    }
}

int
Gnucash::add_quotes (const bo_str& uri)
{
    int rv{};
    gnc_prefs_init ();
    qof_event_suspend();

    auto session = gnc_get_current_session();
    if (!session)
        return 1;

    qof_session_begin(session, uri->c_str(), SESSION_NORMAL_OPEN);
    if (qof_session_get_error(session) != ERR_BACKEND_NO_ERR)
        return cleanup_and_exit_with_failure (session);

    qof_session_load(session, NULL);
    if (qof_session_get_error(session) != ERR_BACKEND_NO_ERR)
        return cleanup_and_exit_with_failure (session);

    try
    {
        GncQuotes quotes;
        std::cout << bl::format (bl::translate ("Found Finance::Quote version {1}.")) % quotes.version() << std::endl;
        auto quote_sources = quotes.sources();
        gnc_quote_source_set_fq_installed (quotes.version().c_str(), quote_sources);
        quotes.fetch(qof_session_get_book(session));
        if (quotes.had_failures())
        {
            std::cerr << quotes.report_failures() << std::endl;
            rv = 2;
        }
    }
    catch (const GncQuoteException& err)
    {
        std::cerr << bl::translate("Price retrieval failed: ") << err.what() << std::endl;
    }
    qof_session_save(session, NULL);
    if (qof_session_get_error(session) != ERR_BACKEND_NO_ERR)
        return cleanup_and_exit_with_failure (session);

    qof_session_destroy(session);
    qof_event_resume();
    return rv;
}

int
Gnucash::report_quotes (const char* source, const StrVec& commodities, bool verbose)
{
    gnc_prefs_init();
    try
    {
        GncQuotes quotes;
        quotes.report(source, commodities, verbose);
        if (quotes.had_failures())
            std::cerr << quotes.report_failures() << std::endl;
    }
    catch (const GncQuoteException& err)
    {
        std::cerr << bl::translate("Price retrieval failed: ") << err.what() << std::endl;
        return -1;
   }
    return 0;
}

int
Gnucash::run_report (const bo_str& file_to_load,
                     const bo_str& run_report,
                     const bo_str& export_type,
                     const bo_str& output_file)
{
    auto args = run_report_args { file_to_load ? *file_to_load : empty_string,
                                  run_report ? *run_report : empty_string,
                                  export_type ? *export_type : empty_string,
                                  output_file ? *output_file : empty_string };
    if (run_report && !run_report->empty())
        scm_boot_guile (0, nullptr, scm_run_report, &args);

    return 0;
}

int
Gnucash::report_show (const bo_str& file_to_load,
                      const bo_str& show_report)
{
    auto args = show_report_args { file_to_load ? *file_to_load : empty_string,
                                   show_report ? *show_report : empty_string };
    if (show_report && !show_report->empty())
        scm_boot_guile (0, nullptr, scm_report_show, &args);

    return 0;
}

int
Gnucash::report_list (void)
{
    scm_boot_guile (0, nullptr, scm_report_list, NULL);
    return 0;
}

namespace {

/* --import-fincosys-sync accepts two different, unrelated JSON schemas:
 *  - "fincosys-ecosystem-sync/v1" (gnc-fincosys-bridge.h): top-level
 *    "schema"/"source"/"atoms"/"links", numeric-string atom ids.
 *  - cognitive_atoms.json (gnc-cognitive-fincosys-loader.h): top-level
 *    "schema_version"/"atoms"/"evaluations", symbolic atom ids like
 *    "entity:RST".
 * Both happen to have a top-level "atoms" key, but with incompatible
 * shapes, so "atoms" can't be used to tell them apart. "schema_version" is
 * the one key unique to the second schema -- the first never has it. This
 * is a lightweight substring sniff rather than a full JSON parse: good
 * enough to route between exactly these two known, already-parsed-in-full-
 * downstream schemas without a third JSON parser just for detection. */
bool
looks_like_cognitive_atoms_json (const gchar *contents)
{
    return contents != nullptr && std::strstr (contents, "\"schema_version\"") != nullptr;
}

} // namespace

int
Gnucash::import_fincosys_sync (const bo_str& sync_file, const bo_str& export_file)
{
    if (!sync_file || sync_file->empty ())
    {
        std::cerr << _("Missing --import-fincosys-sync file parameter") << std::endl;
        return 1;
    }

    gchar *contents = nullptr;
    GError *error = nullptr;
    if (!g_file_get_contents (sync_file->c_str (), &contents, nullptr, &error))
    {
        std::cerr << bl::format (bl::translate ("Failed to read fincosys sync file {1}: {2}"))
                      % *sync_file % (error ? error->message : "unknown error") << std::endl;
        if (error)
            g_error_free (error);
        return 1;
    }

    if (!gnc_cognitive_accounting_init ())
    {
        std::cerr << _("Failed to initialize the cognitive AtomSpace") << std::endl;
        g_free (contents);
        return 1;
    }

    bool is_cognitive_atoms_json = looks_like_cognitive_atoms_json (contents);
    gint n_imported;
    if (is_cognitive_atoms_json)
    {
        GncCognitiveFincosysLoadResult result;
        gboolean ok = gnc_cognitive_load_fincosys_atoms (contents, &result);
        g_free (contents);

        if (!ok)
        {
            std::cerr << _("cognitive_atoms.json file could not be parsed, or the "
                "cognitive AtomSpace failed to initialize; nothing imported.") << std::endl;
            gnc_cognitive_accounting_shutdown ();
            return 1;
        }

        std::cout << bl::format (bl::translate (
            "Imported {1} concept node(s), {2} inheritance link(s), and {3} evaluation "
            "link(s) from cognitive_atoms.json ({4} atom(s), {5} link(s), and {6} "
            "evaluation entry/entries skipped)."))
                      % result.atoms_created % result.links_created % result.evaluations_created
                      % result.atoms_skipped % result.links_skipped % result.evaluations_skipped
                      << std::endl;

        n_imported = static_cast<gint> (result.atoms_created + result.links_created +
                                         result.evaluations_created);
    }
    else
    {
        n_imported = gnc_cognitive_import_fincosys_json (contents);
        g_free (contents);
    }

    if (n_imported < 0)
    {
        std::cerr << _("Fincosys sync file could not be parsed, or the cognitive "
            "AtomSpace failed to initialize; nothing imported.") << std::endl;
        gnc_cognitive_accounting_shutdown ();
        return 1;
    }

    if (!is_cognitive_atoms_json)
        std::cout << bl::format (bl::translate (
            "Imported {1} atom(s)/link(s) from fincosys sync data into the cognitive AtomSpace."))
                      % n_imported << std::endl;

    int rv = 0;
    if (export_file && !export_file->empty ())
    {
        /* Re-exporting the freshly-imported atoms lets this command double as
         * a round-trip check, and produces a merged snapshot (imported atoms
         * plus whatever the cognitive engine already held) that can be fed
         * back into fincosys-atomspace-builder or gnucashm for the next sync
         * pass -- the cognitive AtomSpace itself has no on-disk persistence
         * of its own (see gnc-cognitive-accounting.h), so this is the only
         * way its state survives past process exit. */
        gchar *out_json = gnc_cognitive_export_fincosys_json ();
        if (out_json == nullptr)
        {
            std::cerr << _("Cognitive AtomSpace export failed unexpectedly after a "
                "successful import.") << std::endl;
            rv = 1;
        }
        else
        {
            GError *werror = nullptr;
            if (!g_file_set_contents (export_file->c_str (), out_json, -1, &werror))
            {
                std::cerr << bl::format (bl::translate ("Failed to write {1}: {2}"))
                              % *export_file % (werror ? werror->message : "unknown error") << std::endl;
                if (werror)
                    g_error_free (werror);
                rv = 1;
            }
            else
                std::cout << bl::format (bl::translate ("Wrote merged cognitive AtomSpace to {1}."))
                              % *export_file << std::endl;
            g_free (out_json);
        }
    }

    gnc_cognitive_accounting_shutdown ();
    return rv;
}

int
Gnucash::cognitive_dump_state (const bo_str& output_file)
{
    if (!gnc_cognitive_accounting_init ())
    {
        std::cerr << _("Failed to initialize the cognitive AtomSpace") << std::endl;
        return 1;
    }

    gchar *json = gnc_cognitive_dump_state_json ();
    if (!json)
    {
        std::cerr << _("Failed to dump cognitive state") << std::endl;
        gnc_cognitive_accounting_shutdown ();
        return 1;
    }

    int rv = 0;
    if (output_file && !output_file->empty ())
    {
        GError *error = nullptr;
        if (!g_file_set_contents (output_file->c_str (), json, -1, &error))
        {
            std::cerr << bl::format (bl::translate ("Failed to write {1}: {2}"))
                          % *output_file % (error ? error->message : "unknown error")
                      << std::endl;
            if (error)
                g_error_free (error);
            rv = 1;
        }
        else
            std::cout << bl::format (bl::translate ("Wrote cognitive state to {1}."))
                          % *output_file << std::endl;
    }
    else
        std::cout << json;

    g_free (json);
    gnc_cognitive_accounting_shutdown ();
    return rv;
}

int
Gnucash::cognitive_capability_report (void)
{
    gchar *report = gnc_cognitive_capability_report ();
    if (!report)
        return 1;
    std::cout << report;
    g_free (report);
    return 0;
}

int
Gnucash::cognitive_validate_book (const bo_str& file_to_load,
                                  const bo_str& output_file)
{
    gnc_prefs_init ();
    qof_event_suspend ();

    if (!gnc_cognitive_accounting_init ())
    {
        std::cerr << _("Failed to initialize the cognitive AtomSpace") << std::endl;
        qof_event_resume ();
        return 1;
    }

    QofSession *session = nullptr;
    QofBook *book = nullptr;
    gint mapped = 0;
    gint validated = 0;
    gdouble conf_sum = 0.0;

    if (file_to_load && !file_to_load->empty ())
    {
        session = gnc_get_current_session ();
        if (!session)
        {
            gnc_cognitive_accounting_shutdown ();
            qof_event_resume ();
            return 1;
        }
        qof_session_begin (session, file_to_load->c_str (), SESSION_READ_ONLY);
        if (qof_session_get_error (session) != ERR_BACKEND_NO_ERR)
        {
            std::cerr << _("Failed to open data file for cognitive validation") << std::endl;
            gnc_cognitive_accounting_shutdown ();
            qof_event_resume ();
            return cleanup_and_exit_with_failure (session);
        }
        qof_session_load (session, nullptr);
        if (qof_session_get_error (session) != ERR_BACKEND_NO_ERR)
        {
            std::cerr << _("Failed to load data file for cognitive validation") << std::endl;
            gnc_cognitive_accounting_shutdown ();
            qof_event_resume ();
            return cleanup_and_exit_with_failure (session);
        }
        book = qof_session_get_book (session);
        if (book)
            mapped = gnc_book_to_atomspace (book);
    }

    /* Sample up to a modest number of recent transactions if a book is open. */
    if (book)
    {
        Account *root = gnc_book_get_root_account (book);
        if (root)
        {
            GList *accounts = gnc_account_get_descendants (root);
            for (GList *an = accounts; an; an = an->next)
            {
                Account *acc = GNC_ACCOUNT (an->data);
                if (!acc)
                    continue;
                for (GList *sn = xaccAccountGetSplitList (acc); sn; sn = sn->next)
                {
                    Split *split = GNC_SPLIT (sn->data);
                    Transaction *tx = xaccSplitGetParent (split);
                    if (!tx)
                        continue;
                    gdouble c = gnc_pln_validate_double_entry (tx);
                    conf_sum += c;
                    validated++;
                    if (validated >= 500)
                        break;
                }
                if (validated >= 500)
                    break;
            }
            g_list_free (accounts);
        }
    }

    gdouble avg = validated > 0 ? conf_sum / validated : 0.0;
    GString *s = g_string_new (nullptr);
    g_string_append (s, "{\n");
    g_string_append (s, "  \"schema\": \"gnucashcog-validate/v1\",\n");
    g_string_append_printf (s, "  \"accounts_mapped\": %d,\n", mapped);
    g_string_append_printf (s, "  \"transactions_validated\": %d,\n", validated);
    g_string_append_printf (s, "  \"average_pln_confidence\": %.6f,\n", avg);
    g_string_append_printf (s, "  \"atom_count\": %u\n", gnc_atomspace_count_atoms ());
    g_string_append (s, "}\n");

    int rv = 0;
    if (output_file && !output_file->empty ())
    {
        GError *error = nullptr;
        if (!g_file_set_contents (output_file->c_str (), s->str, -1, &error))
        {
            std::cerr << bl::format (bl::translate ("Failed to write {1}: {2}"))
                          % *output_file % (error ? error->message : "unknown error")
                      << std::endl;
            if (error)
                g_error_free (error);
            rv = 1;
        }
    }
    else
        std::cout << s->str;

    g_string_free (s, TRUE);
    gnc_cognitive_accounting_shutdown ();
    if (session)
        qof_session_destroy (session);
    qof_event_resume ();
    return rv;
}
