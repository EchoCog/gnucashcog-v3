/********************************************************************\
 * gnc-fincosys-bridge.cpp -- Fincosys ecosystem sync bridge          *
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

#include "gnc-fincosys-bridge.h"
#include "gnc-cognitive-accounting.h"

#include <cctype>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{

/*
 * Minimal JSON value type + parser, scoped to the shape of the Fincosys
 * Ecosystem Sync Schema v1 (nested objects/arrays of strings, numbers,
 * booleans and null -- see fincosys-atomspace-builder's README). Mirrors
 * the parser in gnucashm's gnc-fincosys-sync.cpp; duplicated here rather
 * than shared since the two engines are separate codebases with no
 * common dependency to host it in.
 */
class JsonValue
{
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

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
    static JsonValue make_bool (bool b)
    {
        JsonValue v;
        v.m_type = Type::Bool;
        v.m_bool = b;
        return v;
    }

    Type type () const { return m_type; }
    bool is_object () const { return m_type == Type::Object; }
    bool is_array () const { return m_type == Type::Array; }

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

    bool get_bool (const std::string &key, bool def = false) const
    {
        auto *v = find (key);
        return (v && v->m_type == Type::Bool) ? v->m_bool : def;
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
    bool m_bool = false;
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
                out = JsonValue::make_bool (true);
                return true;
            case 'f':
                if (!literal ("false"))
                    return false;
                out = JsonValue::make_bool (false);
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
                        /* Minimal support: the sync schema doesn't use
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

struct AtomRecord
{
    GncAtomHandle handle;
    GncAtomType type;
    std::string name;
    gdouble strength;
    gdouble confidence;
};

const char *
atom_type_name(GncAtomType type)
{
    switch (type)
    {
        case GNC_ATOM_CONCEPT_NODE: return "ConceptNode";
        case GNC_ATOM_PREDICATE_NODE: return "PredicateNode";
        case GNC_ATOM_SCHEMA_NODE: return "SchemaNode";
        case GNC_ATOM_GROUNDED_SCHEMA: return "GroundedSchemaNode";
        case GNC_ATOM_INHERITANCE_LINK: return "InheritanceLink";
        case GNC_ATOM_SIMILARITY_LINK: return "SimilarityLink";
        case GNC_ATOM_MEMBER_LINK: return "MemberLink";
        case GNC_ATOM_EVALUATION_LINK: return "EvaluationLink";
        case GNC_ATOM_EXECUTION_LINK: return "ExecutionLink";
        case GNC_ATOM_IMPLICATION_LINK: return "ImplicationLink";
        case GNC_ATOM_AND_LINK: return "AndLink";
        case GNC_ATOM_OR_LINK: return "OrLink";
        case GNC_ATOM_COMBO_NODE: return "ComboNode";
        default: return "ConceptNode";
    }
}

bool
is_link_type(GncAtomType type)
{
    switch (type)
    {
        case GNC_ATOM_INHERITANCE_LINK:
        case GNC_ATOM_SIMILARITY_LINK:
        case GNC_ATOM_MEMBER_LINK:
        case GNC_ATOM_EVALUATION_LINK:
        case GNC_ATOM_EXECUTION_LINK:
        case GNC_ATOM_IMPLICATION_LINK:
        case GNC_ATOM_AND_LINK:
        case GNC_ATOM_OR_LINK:
            return true;
        default:
            return false;
    }
}

/* Recovers the two participant handles encoded in a link atom's name (see
 * gnc_atomspace_create_inheritance_link()/create_evaluation_link()/
 * create_hierarchy_link() in gnc-cognitive-accounting.cpp, which encode
 * participants as decimal numbers embedded in the name since the simulated
 * AtomSpace doesn't otherwise track link endpoints). Returns false unless
 * exactly two numbers are found. */
bool
extract_two_handles(const std::string &name, guint64 &first, guint64 &second)
{
    std::vector<guint64> numbers;
    size_t i = 0;
    while (i < name.size())
    {
        if (std::isdigit(static_cast<unsigned char>(name[i])))
        {
            size_t start = i;
            while (i < name.size() && std::isdigit(static_cast<unsigned char>(name[i])))
                ++i;
            numbers.push_back(std::stoull(name.substr(start, i - start)));
        }
        else
        {
            ++i;
        }
    }

    if (numbers.size() != 2)
        return false;

    first = numbers[0];
    second = numbers[1];
    return true;
}

/* Several link kinds share the same GncAtomType via legacy aliases (e.g.
 * GNC_ATOM_ACCOUNT_HIERARCHY == GNC_ATOM_INHERITANCE_LINK) but encode their
 * two participant handles in a different order:
 *   - "HierarchyLink:<parent>-><child>"    (gnc_atomspace_create_hierarchy_link)
 *   - "InheritanceLink:<child>-><parent>"  (gnc_atomspace_create_inheritance_link)
 *   - "EvaluationLink:<predicate>:<account>" (gnc_atomspace_create_evaluation_link)
 * Prefer the name-encoded prefix over the atom type so these aren't
 * conflated, falling back to the type-derived name for link kinds that
 * don't have a distinct encoding of their own. */
std::string
derive_link_type(GncAtomType type, const std::string &name)
{
    size_t colon = name.find(':');
    if (colon != std::string::npos)
    {
        std::string prefix = name.substr(0, colon);
        if (prefix == "HierarchyLink" || prefix == "InheritanceLink" || prefix == "EvaluationLink")
            return prefix;
    }
    return atom_type_name(type);
}

void
roles_for_link_type(const std::string &link_type, const char *&role_a, const char *&role_b)
{
    if (link_type == "HierarchyLink")
    {
        role_a = "parent";
        role_b = "child";
    }
    else if (link_type == "EvaluationLink")
    {
        role_a = "predicate";
        role_b = "account";
    }
    else
    {
        /* InheritanceLink and all other link kinds. */
        role_a = "child";
        role_b = "parent";
    }
}

std::string
json_escape(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            default:
                if (c < 0x20)
                {
                    char buf[8];
                    g_snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                }
                else
                {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string
json_quote(const std::string &s) { return "\"" + json_escape(s) + "\""; }

void
collect_atom_cb(GncAtomHandle handle, GncAtomType type, const char *name,
                gdouble strength, gdouble confidence, gpointer user_data)
{
    auto *records = static_cast<std::vector<AtomRecord> *>(user_data);
    records->push_back(AtomRecord{handle, type, name ? name : "", strength, confidence});
}

} // namespace

gchar *
gnc_cognitive_export_fincosys_json(void)
{
    std::vector<AtomRecord> records;
    if (!gnc_atomspace_foreach_atom(collect_atom_cb, &records))
        return nullptr;

    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": \"fincosys-ecosystem-sync/v1\",\n";
    out << "  \"source\": \"gnucashcog-v3\",\n";
    out << "  \"atoms\": [";

    bool first_atom = true;
    for (const auto &rec : records)
    {
        if (is_link_type(rec.type))
            continue;

        out << (first_atom ? "\n" : ",\n");
        first_atom = false;

        out << "    {\n";
        out << "      \"id\": " << json_quote(std::to_string(rec.handle)) << ",\n";
        out << "      \"atom_type\": " << json_quote(atom_type_name(rec.type)) << ",\n";
        out << "      \"label\": " << json_quote(rec.name) << ",\n";
        out << "      \"truth_value\": {\"strength\": " << rec.strength
            << ", \"confidence\": " << rec.confidence << "}\n";
        out << "    }";
    }
    if (!first_atom)
        out << "\n  ";
    out << "],\n";

    out << "  \"links\": [";
    bool first_link = true;
    for (const auto &rec : records)
    {
        if (!is_link_type(rec.type))
            continue;

        guint64 a = 0, b = 0;
        if (!extract_two_handles(rec.name, a, b))
            continue;

        out << (first_link ? "\n" : ",\n");
        first_link = false;

        std::string link_type = derive_link_type(rec.type, rec.name);
        const char *role_a;
        const char *role_b;
        roles_for_link_type(link_type, role_a, role_b);

        out << "    {\n";
        out << "      \"id\": " << json_quote(std::to_string(rec.handle)) << ",\n";
        out << "      \"link_type\": " << json_quote(link_type) << ",\n";
        out << "      \"atoms\": [" << json_quote(std::to_string(a)) << ", "
            << json_quote(std::to_string(b)) << "],\n";
        out << "      \"roles\": {" << json_quote(std::to_string(a)) << ": "
            << json_quote(role_a) << ", " << json_quote(std::to_string(b)) << ": "
            << json_quote(role_b) << "},\n";
        out << "      \"truth_value\": {\"strength\": " << rec.strength
            << ", \"confidence\": " << rec.confidence << "}\n";
        out << "    }";
    }
    if (!first_link)
        out << "\n  ";
    out << "]\n";

    out << "}\n";

    return g_strdup(out.str().c_str());
}

namespace
{

/* Given a link's "roles" object (document atom-id string -> role name
 * string, e.g. {"1000": "child", "1001": "parent"}) find whichever id is
 * tagged @a role_name. Falls back to @a fallback_id when "roles" is
 * absent, isn't an object, or no entry names @a role_name -- so a link
 * still imports (with a best-effort/positional pairing) even against a
 * producer that omits "roles". */
std::string
find_id_with_role (const JsonValue *roles, const std::string &role_name,
                    const std::string &fallback_id)
{
    if (roles == nullptr || !roles->is_object ())
        return fallback_id;

    for (const auto &entry : roles->object_items ())
    {
        if (entry.second.as_string () == role_name)
            return entry.first;
    }
    return fallback_id;
}

} // namespace

gint
gnc_cognitive_import_fincosys_json (const gchar *json)
{
    g_return_val_if_fail (json != nullptr, -1);

    /* gnc_atomspace_foreach_atom() returns FALSE precisely when the
     * cognitive AtomSpace hasn't been initialized (see
     * gnc_cognitive_accounting_init()) -- probe it the same way the export
     * side does before creating anything. Without this check, every
     * creation call below would silently return handle 0 (per their doc
     * comments) and still get counted as imported, so a caller could see a
     * positive count with no real graph behind it. */
    if (!gnc_atomspace_foreach_atom (
            [](GncAtomHandle, GncAtomType, const char *, gdouble, gdouble, gpointer) {},
            nullptr))
    {
        g_warning ("gnc_cognitive_import_fincosys_json: cognitive AtomSpace "
                   "not initialized");
        return -1;
    }

    JsonValue root;
    JsonParser parser (json);
    if (!parser.parse (root) || !root.is_object ())
    {
        g_warning ("gnc_cognitive_import_fincosys_json: invalid JSON document");
        return -1;
    }

    const JsonValue *atoms = root.find ("atoms");
    const JsonValue *links = root.find ("links");
    if ((atoms == nullptr || !atoms->is_array ()) &&
        (links == nullptr || !links->is_array ()))
        return 0;

    /* Remap each document-supplied "id" onto a freshly created local
     * GncAtomHandle -- imported ids belong to a different AtomSpace
     * instance's numbering and must never be reused directly. */
    std::map<std::string, GncAtomHandle> handle_by_doc_id;
    gint count = 0;

    if (atoms != nullptr && atoms->is_array ())
    {
        for (const JsonValue &atom_val : atoms->items ())
        {
            if (!atom_val.is_object ())
                continue;

            std::string doc_id = atom_val.get_string ("id");
            std::string atom_type = atom_val.get_string ("atom_type");
            std::string label = atom_val.get_string ("label");
            if (doc_id.empty ())
                continue;

            GncAtomHandle handle;
            if (atom_type == "ConceptNode")
                handle = gnc_atomspace_create_concept_node (label.c_str ());
            else if (atom_type == "PredicateNode")
                handle = gnc_atomspace_create_predicate_node (label.c_str ());
            else
            {
                /* No public creation function exists for SchemaNode,
                 * GroundedSchemaNode, ComboNode, etc. -- skip rather than
                 * fabricate a node of the wrong kind. */
                g_warning ("gnc_cognitive_import_fincosys_json: unsupported "
                           "atom_type '%s' for id '%s', skipping",
                           atom_type.c_str (), doc_id.c_str ());
                continue;
            }

            if (handle == 0)
            {
                /* Creation functions return handle 0 on failure (e.g. the
                 * AtomSpace vanished mid-import) -- don't record a bogus
                 * mapping or count a node that was never materialized. */
                g_warning ("gnc_cognitive_import_fincosys_json: failed to "
                           "create %s for id '%s', skipping",
                           atom_type.c_str (), doc_id.c_str ());
                continue;
            }

            const JsonValue *tv = atom_val.find ("truth_value");
            gdouble strength = tv ? tv->get_number ("strength", 1.0) : 1.0;
            gdouble confidence = tv ? tv->get_number ("confidence", 1.0) : 1.0;
            gnc_atomspace_set_truth_value (handle, strength, confidence);

            handle_by_doc_id[doc_id] = handle;
            ++count;
        }
    }

    if (links != nullptr && links->is_array ())
    {
        for (const JsonValue &link_val : links->items ())
        {
            if (!link_val.is_object ())
                continue;

            std::string link_type = link_val.get_string ("link_type");
            const JsonValue *link_atoms = link_val.find ("atoms");
            if (link_atoms == nullptr || !link_atoms->is_array () ||
                link_atoms->items ().size () != 2)
                continue;

            std::string positional_a = link_atoms->items ()[0].as_string ();
            std::string positional_b = link_atoms->items ()[1].as_string ();
            const JsonValue *roles = link_val.find ("roles");

            /* Resolve which document id plays which named role, per link
             * kind -- matching roles_for_link_type() on the export side.
             * Falls back to positional_a/positional_b when "roles" doesn't
             * resolve the role, so links still import against a producer
             * that omits "roles" entirely. The positional fallback order
             * itself is link-kind-specific: the export side emits
             * "atoms": [child, parent] for InheritanceLink but
             * [parent, child] for HierarchyLink (see derive_link_type() /
             * roles_for_link_type() above) -- using the same fallback order
             * for both would silently swap HierarchyLink's endpoints
             * whenever "roles" is absent. */
            std::string child_id, parent_id, predicate_id, account_id;
            std::string doc_id_a, doc_id_b;
            if (link_type == "EvaluationLink")
            {
                predicate_id = find_id_with_role (roles, "predicate", positional_a);
                account_id = find_id_with_role (roles, "account", positional_b);
                doc_id_a = predicate_id;
                doc_id_b = account_id;
            }
            else if (link_type == "HierarchyLink")
            {
                parent_id = find_id_with_role (roles, "parent", positional_a);
                child_id = find_id_with_role (roles, "child", positional_b);
                doc_id_a = child_id;
                doc_id_b = parent_id;
            }
            else /* InheritanceLink (and any other link type defaulting to
                  * this pairing) -- only the creation function differs
                  * below (child-first vs parent-first argument order). */
            {
                child_id = find_id_with_role (roles, "child", positional_a);
                parent_id = find_id_with_role (roles, "parent", positional_b);
                doc_id_a = child_id;
                doc_id_b = parent_id;
            }

            auto it_a = handle_by_doc_id.find (doc_id_a);
            auto it_b = handle_by_doc_id.find (doc_id_b);
            if (it_a == handle_by_doc_id.end () || it_b == handle_by_doc_id.end ())
            {
                /* Participant wasn't in this document's "atoms" array (or
                 * was an unsupported atom_type we skipped above) -- the
                 * link can't be reconstructed without both endpoints. */
                g_warning ("gnc_cognitive_import_fincosys_json: link_type "
                           "'%s' references an unresolved participant, "
                           "skipping", link_type.c_str ());
                continue;
            }

            GncAtomHandle handle;
            if (link_type == "EvaluationLink")
                handle = gnc_atomspace_create_evaluation_link (it_a->second, it_b->second, 1.0);
            else if (link_type == "HierarchyLink")
                handle = gnc_atomspace_create_hierarchy_link (it_b->second, it_a->second);
            else if (link_type == "InheritanceLink")
                handle = gnc_atomspace_create_inheritance_link (it_a->second, it_b->second);
            else
            {
                /* No public creation function for SimilarityLink,
                 * MemberLink, ExecutionLink, ImplicationLink, AndLink,
                 * OrLink, etc. */
                g_warning ("gnc_cognitive_import_fincosys_json: unsupported "
                           "link_type '%s', skipping", link_type.c_str ());
                continue;
            }

            if (handle == 0)
            {
                /* Link-creation functions return handle 0 on failure --
                 * don't count a link that was never materialized. */
                g_warning ("gnc_cognitive_import_fincosys_json: failed to "
                           "create %s, skipping", link_type.c_str ());
                continue;
            }

            const JsonValue *tv = link_val.find ("truth_value");
            gdouble strength = tv ? tv->get_number ("strength", 1.0) : 1.0;
            gdouble confidence = tv ? tv->get_number ("confidence", 1.0) : 1.0;
            gnc_atomspace_set_truth_value (handle, strength, confidence);

            ++count;
        }
    }

    return count;
}
