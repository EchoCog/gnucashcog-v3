/********************************************************************\
 * gnc-ko6ml-atomspace.h -- ko6ml Fragment -> AtomSpace Bridge     *
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
 * @file gnc-ko6ml-atomspace.h
 * @brief Phase 1: Instantiation of ko6ml hypergraph fragments in the
 *        cognitive AtomSpace
 *
 * Bridges the engine-independent ko6ml translation layer
 * (gnc-ko6ml-translation.h) to the live cognitive AtomSpace managed
 * by gnc-cognitive-accounting.h.  Fragments are materialised as real
 * ConceptNode / PredicateNode atoms plus InheritanceLink /
 * EvaluationLink relations with the fragment's truth values.
 */

#ifndef GNC_KO6ML_ATOMSPACE_H
#define GNC_KO6ML_ATOMSPACE_H

#include "gnc-ko6ml-translation.h"
#include "gnc-cognitive-accounting.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup Ko6mlTranslation
 * @{
 */

/** Instantiate a ko6ml hypergraph fragment in the cognitive
 *  AtomSpace.  Every atom of the fragment is created via the
 *  AtomSpace API; node truth values are applied with
 *  gnc_atomspace_set_truth_value().
 * @param fragment Fragment produced by
 *        gnc_ko6ml_primitive_to_hypergraph()
 * @param atom_handles Optional output array (caller frees with
 *        g_free) receiving one GncAtomHandle per fragment atom, in
 *        fragment order
 * @param handle_count Optional output for the number of handles
 * @return Handle of the fragment's root concept atom, or 0 on
 *         failure (invalid fragment or uninitialized AtomSpace) */
GncAtomHandle gnc_ko6ml_fragment_to_atomspace(
    const GncKo6mlHypergraphFragment *fragment,
    GncAtomHandle **atom_handles,
    guint *handle_count);

/** Translate a ko6ml primitive directly into the cognitive
 *  AtomSpace (convenience wrapper composing
 *  gnc_ko6ml_primitive_to_hypergraph() and
 *  gnc_ko6ml_fragment_to_atomspace()).
 * @param primitive Primitive to instantiate
 * @return Handle of the primitive's root concept atom, or 0 on
 *         failure */
GncAtomHandle gnc_ko6ml_primitive_to_atomspace(
    const GncKo6mlPrimitive *primitive);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* GNC_KO6ML_ATOMSPACE_H */
