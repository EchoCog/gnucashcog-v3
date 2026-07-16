/********************************************************************\
 * gnc-fincosys-bridge.h -- Fincosys ecosystem sync bridge            *
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
/** @file gnc-fincosys-bridge.h
 *  @brief Export the cognitive AtomSpace (gnc-cognitive-accounting.h) to
 *         the shared "fincosys-ecosystem-sync/v1" schema.
 *
 *  This is the integration point used by the external
 *  ``fincosys-atomspace-builder`` repository (see its
 *  ``atomspace_builder/loaders/gnucashcog.py``) to pull gnucashcog-v3's
 *  ConceptNode/PredicateNode/InheritanceLink/etc. atoms into a real
 *  neuro-symbolic hypergraph, alongside records synced from fincosys,
 *  gnucashm, and helix.
 */

#ifndef GNC_FINCOSYS_BRIDGE_H
#define GNC_FINCOSYS_BRIDGE_H

#include <glib.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Export every atom currently registered in the cognitive AtomSpace to the
 *  shared Fincosys Ecosystem Sync Schema v1
 *  (\c "schema": "fincosys-ecosystem-sync/v1", \c "source": "gnucashcog-v3").
 *
 *  Node-type atoms (ConceptNode, PredicateNode, SchemaNode,
 *  GroundedSchemaNode, ComboNode) are exported under \c "atoms"; link-type
 *  atoms (InheritanceLink, EvaluationLink, SimilarityLink, MemberLink,
 *  ExecutionLink, ImplicationLink, AndLink, OrLink) are exported under
 *  \c "links", with participant handles recovered on a best-effort basis
 *  from the link atom's encoded name (see
 *  gnc_atomspace_create_inheritance_link() /
 *  gnc_atomspace_create_evaluation_link() in gnc-cognitive-accounting.cpp,
 *  which encode participant handles as decimal numbers in the name since
 *  the simulated AtomSpace doesn't otherwise track link endpoints).
 *  Links whose participants can't be recovered are omitted.
 *
 *  @return Newly allocated JSON string (caller frees with g_free()), or
 *          NULL if the cognitive AtomSpace has not been initialized
 *          (see gnc_cognitive_accounting_init()).
 */
gchar *gnc_cognitive_export_fincosys_json (void);

/** Parse a JSON document matching the shared Fincosys Ecosystem Sync
 *  Schema v1 (as produced by gnc_cognitive_export_fincosys_json(),
 *  gnucashm's gnc_organizations_to_fincosys_json(), or by
 *  fincosys-atomspace-builder's EcosystemSyncExporter) and materialize its
 *  \c "atoms" and \c "links" into the cognitive AtomSpace, remapping each
 *  document-supplied \c "id" onto a freshly created local
 *  ::GncAtomHandle (the ids in an imported document belong to a
 *  different AtomSpace instance's numbering and cannot be reused
 *  directly).
 *
 *  Only atom/link kinds with a public creation function are
 *  reconstructed: \c "ConceptNode" and \c "PredicateNode" atoms, and
 *  \c "InheritanceLink" / \c "EvaluationLink" / \c "HierarchyLink" links
 *  (via gnc_atomspace_create_concept_node(),
 *  gnc_atomspace_create_predicate_node(),
 *  gnc_atomspace_create_inheritance_link(),
 *  gnc_atomspace_create_evaluation_link(), and
 *  gnc_atomspace_create_hierarchy_link() respectively). Records of any
 *  other type, and links whose participant ids didn't resolve to an
 *  imported atom, are skipped (logged) rather than aborting the whole
 *  import -- this bridge only ever gains ground on each sync, it never
 *  needs to roll back.
 *
 *  Each imported atom's truth value is applied via
 *  gnc_atomspace_set_truth_value() from the document's \c "truth_value"
 *  object, defaulting to (1.0, 1.0) when absent.
 *
 *  @param json The JSON document text.
 *  @return The number of atoms plus links imported, or -1 if @a json
 *          could not be parsed or the cognitive AtomSpace has not been
 *          initialized (see gnc_cognitive_accounting_init()).
 */
gint gnc_cognitive_import_fincosys_json (const gchar *json);

#ifdef __cplusplus
}
#endif

#endif /* GNC_FINCOSYS_BRIDGE_H */
