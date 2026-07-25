/********************************************************************\
 * gnc-cognitive-fincosys-loader.cpp -- Load cognitive_atoms.json     *
 *                                       into the cognitive AtomSpace  *
 * Copyright (C) 2026 GnuCash Cognitive Engine                        *
 *                                                                    *
 * This program is free software; you can redistribute it and/or      *
 * modify it under the terms of the GNU General Public License as     *
 * published by the Free Software Foundation; either version 2 of     *
 * the License, or (at your option) any later version.                *
 *                                                                    *
 * This program is distributed in the hope that it will be useful,    *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      *
 * GNU General Public License for more details.                       *
 *********************************************************************/

#include "gnc-cognitive-fincosys-loader.h"
#include "gnc-cognitive-accounting.h"

#include <cctype>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace
{

/*
 * Minimal JSON value type + parser, scoped to the shape of
 * cognitive_atoms.json (nested objects/arrays of strings, numbers, and
 * null -- see cognitive_bridge.py / FINCOSYS_COGNITIVE_BRIDGE.md). This is
 * a second, independent copy of the same small parser pattern
 * gnc-fincosys-bridge.cpp uses for the *other* ("fincosys-ecosystem-sync/v1")
 * schema; that one lives in an anonymous namespace in its own .cpp and
 * isn't exposed for reuse, and gnc-fincosys-bridge.cpp's own header
 * explains why it duplicates gnucashm's copy rather than sharing it
 * ("the two engines are separate codebases with no common dependency to
 * host it in"). The same reasoning applies one level down here: rather
 * than widen gnc-fincosys-bridge.h's public surface to export its private
 * parser just for this one other file in the same library, this loader
 * gets its own copy, trimmed to only what cognitive_atoms.json needs
 * (no booleans, since that schema never uses them).
 */
class JsonValue
{
public:
    enum class Type { Null, Number, String, Array, Object };

    JsonValue () : m_type (Type::Null) {}

    static JsonValue make_object () { JsonValue v; v.m_type = Type::Object; return v; }
    static JsonValue make_array () { JsonValue v; v.m_type = Type::Array; return v; }
    static JsonValue make_string (std::string s)
    {
        JsonValue v;
        v.m_type = Type::String;
        v.m_string = std::move (s);
        return v;
    }
    static JsonValue make_number (double d)
    {
        JsonValue v;
        v.m_type = Type::Number;
        v.m_number = d;
        return v;
    }

    Type type () const { return m_type; }
    bool is_object () const { return m_type == Type::Object; }
    bool is_array () const { return m_type == Type::Array; }
    bool is_string () const { return m_type == Type::String; }

    const std::vector<JsonValue> &items () const { return m_array; }
    void push_back (JsonValue val) { m_array.push_back (std::move (val)); }

    void set (const std::string &key, JsonValue val) { m_object[key] = std::move (val); }

    const JsonValue *find (const std::string &key) const
    {
        auto it = m_object.find (key);
        return it == m_object.end () ? nullptr : &it->second;
    }

    const std::map<std::string, JsonValue> &object_items () const { return m_object; }

    std::string get_string (const std::string &key, const std::string &def = "") const
    {
        auto *v = find (key);
        return (v && v->m_type == Type::String) ? v->m_string : def;
    }

    double get_number (const std::string &key, double def = 0.0) const
    {
        auto *v = find (key);
        return (v && v->m_type == Type::Number) ? v->m_number : def;
    }

    const JsonValue *find_array (const std::string &key) const
    {
        auto *v = find (key);
        return (v && v->m_type == Type::Array) ? v : nullptr;
    }

    const JsonValue *find_object (const std::string &key) const
    {
        auto *v = find (key);
        return (v && v->m_type == Type::Object) ? v : nullptr;
    }

    /* Direct accessor for a value that IS a string itself (e.g. one
     * element of an array), as opposed to get_string(key) which looks up
     * a string-valued field within an object. */
    std::string as_string (const std::string &def = "") const
    {
        return m_type == Type::String ? m_string : def;
    }

private:
    Type m_type;
    std::string m_string;
    double m_number = 0.0;
    std::vector<JsonValue> m_array;
    std::map<std::string, JsonValue> m_object;
};

class JsonParser
{
public:
    explicit JsonParser (std::string text) : m_text (std::move (text)), m_pos (0) {}

    bool parse (JsonValue &out)
    {
        skip_ws ();
        if (!parse_value (out))
            return false;
        skip_ws ();
        return eof ();
    }

private:
    std::string m_text;
    size_t m_pos;

    void skip_ws ()
    {
        while (m_pos < m_text.size () && std::isspace (static_cast<unsigned char> (m_text[m_pos])))
            ++m_pos;
    }

    bool eof () const { return m_pos >= m_text.size (); }
    char peek () const { return m_text[m_pos]; }

    bool consume (char c)
    {
        skip_ws ();
        if (eof () || m_text[m_pos] != c)
            return false;
        ++m_pos;
        return true;
    }

    bool literal (const char *lit)
    {
        size_t len = std::strlen (lit);
        if (m_text.compare (m_pos, len, lit) == 0)
        {
            m_pos += len;
            return true;
        }
        return false;
    }

    bool parse_value (JsonValue &out)
    {
        skip_ws ();
        if (eof ())
            return false;

        switch (peek ())
        {
            case '{':
                return parse_object (out);
            case '[':
                return parse_array (out);
            case '"':
            {
                std::string s;
                if (!parse_string (s))
                    return false;
                out = JsonValue::make_string (std::move (s));
                return true;
            }
            case 't':
                if (!literal ("true"))
                    return false;
                out = JsonValue::make_number (1.0);
                return true;
            case 'f':
                if (!literal ("false"))
                    return false;
                out = JsonValue::make_number (0.0);
                return true;
            case 'n':
                if (!literal ("null"))
                    return false;
                out = JsonValue ();
                return true;
            default:
                return parse_number (out);
        }
    }

    bool parse_object (JsonValue &out)
    {
        if (!consume ('{'))
            return false;
        out = JsonValue::make_object ();
        skip_ws ();
        if (consume ('}'))
            return true;

        for (;;)
        {
            std::string key;
            skip_ws ();
            if (!parse_string (key))
                return false;
            if (!consume (':'))
                return false;

            JsonValue val;
            if (!parse_value (val))
                return false;
            out.set (key, std::move (val));

            skip_ws ();
            if (consume (','))
                continue;
            if (consume ('}'))
                break;
            return false;
        }
        return true;
    }

    bool parse_array (JsonValue &out)
    {
        if (!consume ('['))
            return false;
        out = JsonValue::make_array ();
        skip_ws ();
        if (consume (']'))
            return true;

        for (;;)
        {
            JsonValue val;
            if (!parse_value (val))
                return false;
            out.push_back (std::move (val));

            skip_ws ();
            if (consume (','))
                continue;
            if (consume (']'))
                break;
            return false;
        }
        return true;
    }

    bool parse_string (std::string &out)
    {
        if (!consume ('"'))
            return false;
        out.clear ();

        while (!eof () && m_text[m_pos] != '"')
        {
            char c = m_text[m_pos++];
            if (c == '\\' && !eof ())
            {
                char esc = m_text[m_pos++];
                switch (esc)
                {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u':
                        /* Minimal support: cognitive_atoms.json doesn't use
                         * non-ASCII codes/keys in practice, so \u escapes
                         * are skipped rather than decoded. */
                        if (m_pos + 4 <= m_text.size ())
                            m_pos += 4;
                        out += '?';
                        break;
                    default:
                        out += esc;
                        break;
                }
            }
            else
            {
                out += c;
            }
        }
        if (eof ())
            return false;
        ++m_pos; /* closing quote */
        return true;
    }

    bool parse_number (JsonValue &out)
    {
        size_t start = m_pos;
        if (!eof () && (peek () == '-' || peek () == '+'))
            ++m_pos;
        while (!eof () &&
               (std::isdigit (static_cast<unsigned char> (peek ())) || peek () == '.' ||
                peek () == 'e' || peek () == 'E' || peek () == '+' || peek () == '-'))
            ++m_pos;
        if (m_pos == start)
            return false;

        std::string numstr = m_text.substr (start, m_pos - start);
        try
        {
            out = JsonValue::make_number (std::stod (numstr));
        }
        catch (...)
        {
            return false;
        }
        return true;
    }
};

/*
 * Generic string-valued attribute store for atoms created by
 * gnc_cognitive_load_fincosys_atoms(), keyed by GncAtomHandle. Local to
 * this loader (the cognitive AtomSpace engine has no such storage of its
 * own) -- mirrors the same pattern gnc-fincosys-bridge.cpp uses for its
 * own, separate attribute table; see that file's comment for why these
 * aren't unified into one shared store. Not exported to
 * gnc_cognitive_export_fincosys_json(), which only ever sees the
 * fincosys-bridge's own table.
 */
std::map<GncAtomHandle, std::map<std::string, std::string>> g_loader_atom_attributes;

} // namespace

const gchar *
gnc_cognitive_fincosys_loader_get_atom_attribute (GncAtomHandle handle, const gchar *key)
{
    if (key == nullptr)
        return nullptr;

    auto handle_it = g_loader_atom_attributes.find (handle);
    if (handle_it == g_loader_atom_attributes.end ())
        return nullptr;

    auto attr_it = handle_it->second.find (key);
    if (attr_it == handle_it->second.end ())
        return nullptr;

    return attr_it->second.c_str ();
}

gboolean
gnc_cognitive_load_fincosys_atoms (const gchar *json, GncCognitiveFincosysLoadResult *result)
{
    if (result != nullptr)
        std::memset (result, 0, sizeof (*result));

    g_return_val_if_fail (json != nullptr, FALSE);

    /* Probe AtomSpace initialization the same way gnc-fincosys-bridge.cpp's
     * import function does: gnc_atomspace_foreach_atom() returns FALSE
     * precisely when the cognitive AtomSpace hasn't been initialized.
     * Without this check, every creation call below would silently return
     * handle 0 (per their doc comments) and still be counted, so a caller
     * could see plausible-looking counts backed by no real graph. */
    if (!gnc_atomspace_foreach_atom (
            [] (GncAtomHandle, GncAtomType, const char *, gdouble, gdouble, gpointer) {},
            nullptr))
    {
        g_warning ("gnc_cognitive_load_fincosys_atoms: cognitive AtomSpace not initialized");
        return FALSE;
    }

    JsonValue root;
    JsonParser parser (json);
    if (!parser.parse (root) || !root.is_object ())
    {
        g_warning ("gnc_cognitive_load_fincosys_atoms: invalid JSON document");
        return FALSE;
    }

    GncCognitiveFincosysLoadResult local_result{};

    /* Maps a document-supplied "id" string (e.g. "entity:RST",
     * "account:BANK-111") onto the GncAtomHandle created for it -- only
     * populated for GNC_ATOM_CONCEPT_NODE entries, since those are the only
     * kind cognitive_atoms.json's "source"/"target"/"participants" fields
     * ever reference. */
    std::map<std::string, GncAtomHandle> handle_by_doc_id;

    const JsonValue *atoms = root.find_array ("atoms");
    if (atoms != nullptr)
    {
        for (const JsonValue &atom_val : atoms->items ())
        {
            if (!atom_val.is_object ())
            {
                ++local_result.atoms_skipped;
                continue;
            }

            std::string atom_type = atom_val.get_string ("atom_type");
            std::string doc_id = atom_val.get_string ("id");

            if (atom_type == "GNC_ATOM_CONCEPT_NODE")
            {
                if (doc_id.empty ())
                {
                    g_warning ("gnc_cognitive_load_fincosys_atoms: "
                               "GNC_ATOM_CONCEPT_NODE entry missing \"id\", skipping");
                    ++local_result.atoms_skipped;
                    continue;
                }

                std::string label = atom_val.get_string ("label", doc_id);
                GncAtomHandle handle = gnc_atomspace_create_concept_node (label.c_str ());
                if (handle == 0)
                {
                    g_warning ("gnc_cognitive_load_fincosys_atoms: failed to create "
                               "concept node for id '%s', skipping", doc_id.c_str ());
                    ++local_result.atoms_skipped;
                    continue;
                }

                handle_by_doc_id[doc_id] = handle;
                ++local_result.atoms_created;

                /* Start from a clean slate for this (freshly created)
                 * handle -- guards against a stale entry from an earlier
                 * load in the same process having reused the same handle
                 * number (the simulated AtomSpace's handle counter resets
                 * across gnc_cognitive_accounting_shutdown()/_init()
                 * cycles). This must run unconditionally, not only when
                 * this entry has an "attributes" object: an entry with no
                 * attributes must still clear out any stale attributes a
                 * prior load left behind under the same reused handle. */
                g_loader_atom_attributes.erase (handle);

                const JsonValue *attrs = atom_val.find_object ("attributes");
                if (attrs != nullptr)
                {
                    for (const auto &kv : attrs->object_items ())
                    {
                        /* Attribute values in this schema are plain
                         * strings or JSON null (e.g. a root account's
                         * "parent_code": null) -- non-string values are
                         * skipped rather than stringified, since this
                         * schema never emits numeric/array attributes. */
                        if (kv.second.is_string ())
                            g_loader_atom_attributes[handle][kv.first] = kv.second.as_string ();
                    }
                }
            }
            else if (atom_type == "GNC_ATOM_INHERITANCE_LINK")
            {
                std::string source_id = atom_val.get_string ("source");
                std::string target_id = atom_val.get_string ("target");

                auto source_it = handle_by_doc_id.find (source_id);
                auto target_it = handle_by_doc_id.find (target_id);
                if (source_id.empty () || target_id.empty () ||
                    source_it == handle_by_doc_id.end () ||
                    target_it == handle_by_doc_id.end ())
                {
                    g_warning ("gnc_cognitive_load_fincosys_atoms: "
                               "GNC_ATOM_INHERITANCE_LINK '%s' references an unresolved "
                               "source ('%s') or target ('%s'), skipping",
                               doc_id.c_str (), source_id.c_str (), target_id.c_str ());
                    ++local_result.links_skipped;
                    continue;
                }

                /* gnc_atomspace_create_inheritance_link(child, parent)
                 * matches cognitive_bridge.py's "source" = child account,
                 * "target" = parent entity/account convention exactly. */
                GncAtomHandle link_handle =
                    gnc_atomspace_create_inheritance_link (source_it->second, target_it->second);
                if (link_handle == 0)
                {
                    g_warning ("gnc_cognitive_load_fincosys_atoms: failed to create "
                               "inheritance link '%s', skipping", doc_id.c_str ());
                    ++local_result.links_skipped;
                    continue;
                }

                ++local_result.links_created;
            }
            else
            {
                g_warning ("gnc_cognitive_load_fincosys_atoms: unsupported atom_type "
                           "'%s' for id '%s', skipping",
                           atom_type.c_str (), doc_id.c_str ());
                ++local_result.atoms_skipped;
            }
        }
    }

    /* One PredicateNode per distinct predicate name across the whole
     * document, rather than one per evaluation entry -- cognitive_atoms.json
     * typically repeats the same predicate (e.g. "balanced_transaction")
     * across every transaction, and there is no reason for each to be a
     * distinct atom. */
    std::map<std::string, GncAtomHandle> predicate_handle_by_name;

    const JsonValue *evaluations = root.find_array ("evaluations");
    if (evaluations != nullptr)
    {
        for (const JsonValue &eval_val : evaluations->items ())
        {
            if (!eval_val.is_object ())
            {
                ++local_result.evaluations_skipped;
                continue;
            }

            std::string doc_id = eval_val.get_string ("id");
            std::string predicate_name = eval_val.get_string ("predicate");
            const JsonValue *participants = eval_val.find_array ("participants");

            if (predicate_name.empty () || participants == nullptr)
            {
                g_warning ("gnc_cognitive_load_fincosys_atoms: evaluation '%s' missing "
                           "\"predicate\" or \"participants\", skipping", doc_id.c_str ());
                ++local_result.evaluations_skipped;
                continue;
            }

            auto pred_it = predicate_handle_by_name.find (predicate_name);
            GncAtomHandle predicate_handle;
            if (pred_it != predicate_handle_by_name.end ())
            {
                predicate_handle = pred_it->second;
            }
            else
            {
                predicate_handle = gnc_atomspace_create_predicate_node (predicate_name.c_str ());
                if (predicate_handle == 0)
                {
                    g_warning ("gnc_cognitive_load_fincosys_atoms: failed to create "
                               "predicate node '%s' for evaluation '%s', skipping",
                               predicate_name.c_str (), doc_id.c_str ());
                    ++local_result.evaluations_skipped;
                    continue;
                }
                predicate_handle_by_name[predicate_name] = predicate_handle;
            }

            const JsonValue *tv = eval_val.find_object ("truth_value");
            gdouble strength = tv ? tv->get_number ("strength", 1.0) : 1.0;
            gdouble confidence = tv ? tv->get_number ("confidence", 1.0) : 1.0;

            guint links_for_this_entry = 0;
            for (const JsonValue &participant_val : participants->items ())
            {
                std::string participant_id = participant_val.as_string ();
                auto participant_it = handle_by_doc_id.find (participant_id);
                if (participant_id.empty () || participant_it == handle_by_doc_id.end ())
                {
                    g_warning ("gnc_cognitive_load_fincosys_atoms: evaluation '%s' "
                               "references an unresolved participant '%s', skipping "
                               "that participant", doc_id.c_str (), participant_id.c_str ());
                    continue;
                }

                /* gnc_atomspace_create_evaluation_link() only connects a
                 * predicate to a single account atom and hard-codes a 0.9
                 * confidence internally -- create the link, then
                 * immediately overwrite its truth value with the JSON
                 * document's own strength/confidence via
                 * gnc_atomspace_set_truth_value() (see the header comment
                 * for why one JSON evaluation entry can yield several
                 * EvaluationLink atoms). */
                GncAtomHandle link_handle = gnc_atomspace_create_evaluation_link (
                    predicate_handle, participant_it->second, strength);
                if (link_handle == 0)
                {
                    g_warning ("gnc_cognitive_load_fincosys_atoms: failed to create "
                               "evaluation link for '%s', skipping", doc_id.c_str ());
                    continue;
                }

                gnc_atomspace_set_truth_value (link_handle, strength, confidence);
                ++links_for_this_entry;
            }

            if (links_for_this_entry == 0)
                ++local_result.evaluations_skipped;
            else
                local_result.evaluations_created += links_for_this_entry;
        }
    }

    if (result != nullptr)
        *result = local_result;

    return TRUE;
}
