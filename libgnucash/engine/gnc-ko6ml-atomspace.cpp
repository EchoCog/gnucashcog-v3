/********************************************************************\
 * gnc-ko6ml-atomspace.cpp -- ko6ml Fragment -> AtomSpace Bridge   *
 * Copyright (C) 2024 GnuCash Cognitive Engine                     *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 ********************************************************************/

/**
 * @file gnc-ko6ml-atomspace.cpp
 * @brief Phase 1: Instantiation of ko6ml hypergraph fragments in the
 *        cognitive AtomSpace
 */

#include <vector>

#include "gnc-ko6ml-atomspace.h"

GncAtomHandle gnc_ko6ml_fragment_to_atomspace(
    const GncKo6mlHypergraphFragment *fragment,
    GncAtomHandle **atom_handles,
    guint *handle_count)
{
    if (atom_handles)
        *atom_handles = NULL;
    if (handle_count)
        *handle_count = 0;

    g_return_val_if_fail(fragment != NULL, 0);
    g_return_val_if_fail(fragment->atoms != NULL, 0);
    g_return_val_if_fail(fragment->atoms->len > 0, 0);

    std::vector<GncAtomHandle> handles(fragment->atoms->len, 0);
    GncAtomHandle root_handle = 0;

    for (guint i = 0; i < fragment->atoms->len; i++) {
        GncKo6mlHypergraphAtom *atom =
            (GncKo6mlHypergraphAtom*)g_ptr_array_index(fragment->atoms, i);
        GncAtomHandle handle = 0;

        switch (atom->atom_type) {
        case GNC_KO6ML_NODE_CONCEPT:
        case GNC_KO6ML_NODE_NUMBER:
            /* NumberNodes are represented as concept atoms carrying
             * the numeric string -- the GncAtomType vocabulary has no
             * dedicated NumberNode type */
            handle = gnc_atomspace_create_concept_node(atom->name);
            if (handle != 0)
                gnc_atomspace_set_truth_value(handle, atom->strength,
                                              atom->confidence);
            break;

        case GNC_KO6ML_NODE_PREDICATE:
            handle = gnc_atomspace_create_predicate_node(atom->name);
            break;

        case GNC_KO6ML_LINK_INHERITANCE:
            if (atom->outgoing_count == 2 &&
                atom->outgoing[0] < i && atom->outgoing[1] < i) {
                handle = gnc_atomspace_create_inheritance_link(
                    handles[atom->outgoing[0]],
                    handles[atom->outgoing[1]]);
            }
            break;

        case GNC_KO6ML_LINK_EVALUATION:
            if (atom->outgoing_count == 2 &&
                atom->outgoing[0] < i && atom->outgoing[1] < i) {
                handle = gnc_atomspace_create_evaluation_link(
                    handles[atom->outgoing[0]],
                    handles[atom->outgoing[1]],
                    atom->strength);
            }
            break;
        }

        if (handle == 0) {
            g_warning("Failed to instantiate ko6ml fragment atom %u "
                      "('%s') in AtomSpace", i, atom->name);
            return 0;
        }

        handles[i] = handle;
        if (root_handle == 0)
            root_handle = handle;
    }

    if (atom_handles) {
        *atom_handles = g_new(GncAtomHandle, handles.size());
        for (gsize i = 0; i < handles.size(); i++)
            (*atom_handles)[i] = handles[i];
    }
    if (handle_count)
        *handle_count = (guint)handles.size();

    return root_handle;
}

GncAtomHandle gnc_ko6ml_primitive_to_atomspace(
    const GncKo6mlPrimitive *primitive)
{
    g_return_val_if_fail(primitive != NULL, 0);

    GncKo6mlHypergraphFragment *fragment =
        gnc_ko6ml_primitive_to_hypergraph(primitive);
    if (!fragment)
        return 0;

    GncAtomHandle root = gnc_ko6ml_fragment_to_atomspace(fragment,
                                                         NULL, NULL);
    gnc_ko6ml_hypergraph_fragment_free(fragment);
    return root;
}
