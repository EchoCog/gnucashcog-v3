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
#include <sstream>
#include <string>
#include <vector>

namespace
{

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

        const char *type_name = atom_type_name(rec.type);
        const char *role_a = (rec.type == GNC_ATOM_EVALUATION_LINK) ? "predicate" : "child";
        const char *role_b = (rec.type == GNC_ATOM_EVALUATION_LINK) ? "account" : "parent";

        out << "    {\n";
        out << "      \"id\": " << json_quote(std::to_string(rec.handle)) << ",\n";
        out << "      \"link_type\": " << json_quote(type_name) << ",\n";
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
