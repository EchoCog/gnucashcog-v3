/********************************************************************\
 * gnc-cognitive-fincosys-loader.h -- Load cognitive_atoms.json into  *
 *                                     the cognitive AtomSpace         *
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
/** @file gnc-cognitive-fincosys-loader.h
 *  @brief Load a "cognitive_atoms.json" document (as produced by
 *         bindings/python/example_scripts/fincosys_sync/cognitive_bridge.py)
 *         into the cognitive AtomSpace (gnc-cognitive-accounting.h).
 *
 *  This is a *different* schema from the one gnc-fincosys-bridge.h handles
 *  ("fincosys-ecosystem-sync/v1"). cognitive_atoms.json is keyed by
 *  top-level "schema_version" + "atoms"/"evaluations", and its "atoms"
 *  entries carry symbolic string ids ("entity:RST", "account:BANK-111", ...)
 *  rather than numeric GncAtomHandle values -- see
 *  FINCOSYS_COGNITIVE_BRIDGE.md in that directory for the exact shape.
 *  Until this file, nothing read that document back into the compiled
 *  engine; this is the loader FINCOSYS_COGNITIVE_BRIDGE.md's "Next Steps"
 *  checklist asked for.
 */

#ifndef GNC_COGNITIVE_FINCOSYS_LOADER_H
#define GNC_COGNITIVE_FINCOSYS_LOADER_H

#include <glib.h>

#include "gnc-cognitive-accounting.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** Per-kind created/skipped counts for a gnc_cognitive_load_fincosys_atoms()
 *  call, so a caller (CLI or test) can report exactly what happened rather
 *  than a single opaque count.
 *
 *  "links" here refers to GNC_ATOM_INHERITANCE_LINK entries, which live
 *  inside the JSON document's "atoms" array alongside GNC_ATOM_CONCEPT_NODE
 *  entries (see cognitive_atoms.json's schema) but are counted separately
 *  here since they go through a different creation call
 *  (gnc_atomspace_create_inheritance_link() vs.
 *  gnc_atomspace_create_concept_node()).
 *
 *  "evaluations_created" counts actual GNC_ATOM_EVALUATION_LINK atoms
 *  materialized, which can exceed the number of JSON "evaluations" entries:
 *  gnc_atomspace_create_evaluation_link() only connects a predicate to a
 *  *single* account atom, so an evaluation entry with N "participants"
 *  produces N EvaluationLink atoms (one per participant), all sharing that
 *  entry's predicate and truth value. "evaluations_skipped" counts JSON
 *  evaluation entries that produced zero links (e.g. no participant id
 *  resolved).
 */
typedef struct
{
    guint atoms_created;         /**< GNC_ATOM_CONCEPT_NODE atoms created */
    guint atoms_skipped;         /**< "atoms" entries skipped (bad/unsupported type, missing id) */
    guint links_created;         /**< GNC_ATOM_INHERITANCE_LINK atoms created */
    guint links_skipped;         /**< inheritance-link entries skipped (unresolved source/target) */
    guint evaluations_created;   /**< GNC_ATOM_EVALUATION_LINK atoms created (see above) */
    guint evaluations_skipped;   /**< "evaluations" entries that produced zero links */
} GncCognitiveFincosysLoadResult;

/** Parse @a json as a cognitive_atoms.json document and materialize it into
 *  the in-process cognitive AtomSpace:
 *
 *  - Each "atoms" entry with `"atom_type": "GNC_ATOM_CONCEPT_NODE"` becomes
 *    a real atom via gnc_atomspace_create_concept_node(), keyed internally
 *    by its document `"id"` string so later entries can reference it. Any
 *    `"attributes"` object is recorded against the new handle (see
 *    gnc_cognitive_fincosys_loader_get_atom_attribute() below -- the
 *    cognitive AtomSpace itself has no generic attribute storage, so this
 *    mirrors the local handle-keyed side table gnc-fincosys-bridge.cpp
 *    already uses for the same reason).
 *  - Each "atoms" entry with `"atom_type": "GNC_ATOM_INHERITANCE_LINK"`
 *    resolves its `"source"`/`"target"` ids against previously-created
 *    concept-node atoms and, if both resolve, creates the link via
 *    gnc_atomspace_create_inheritance_link(source, target) (matching that
 *    function's child-then-parent argument order, and
 *    cognitive_bridge.py's own "source" = child account, "target" = parent
 *    entity/account convention). If either id is unresolved, the entry is
 *    skipped with a g_warning() rather than treated as fatal -- the file is
 *    documented to emit atoms before the links that reference them, but
 *    this does not assume that ordering blindly.
 *  - Each "evaluations" entry (`"atom_type": "GNC_ATOM_EVALUATION_LINK"`)
 *    resolves a predicate atom for its `"predicate"` string (one
 *    PredicateNode per distinct predicate name across the whole call, via
 *    gnc_atomspace_create_predicate_node()) and, for every "participants"
 *    id that resolves to a previously-created concept-node atom, creates
 *    one EvaluationLink via gnc_atomspace_create_evaluation_link() and then
 *    applies the entry's `"truth_value"` (`"strength"`/`"confidence"`) via
 *    gnc_atomspace_set_truth_value(). Unresolved participant ids are
 *    skipped with a g_warning(); an entry with zero resolved participants
 *    creates no links at all.
 *
 *  @param json The JSON document text. Must not be NULL.
 *  @param result Optional out-parameter populated with per-kind
 *         created/skipped counts. May be NULL if the caller doesn't need
 *         the breakdown. Left untouched if this function returns FALSE.
 *  @return TRUE if @a json parsed as a JSON object and the cognitive
 *          AtomSpace was initialized (even if individual entries were
 *          skipped -- see @a result for that detail); FALSE if @a json is
 *          NULL, unparseable, not a JSON object, or the cognitive AtomSpace
 *          has not been initialized (gnc_cognitive_accounting_init()).
 */
gboolean gnc_cognitive_load_fincosys_atoms (const gchar *json,
                                             GncCognitiveFincosysLoadResult *result);

/** Look up a generic attribute recorded for @a handle by a prior
 *  gnc_cognitive_load_fincosys_atoms() call in this process (e.g.
 *  `"entity_code"`, `"account_type"`, `"currency"`, `"parent_code"`).
 *
 *  This is local storage owned by this loader, not part of the cognitive
 *  AtomSpace engine itself (gnc-cognitive-accounting.h has no generic
 *  attribute API) -- see the note on gnc_cognitive_load_fincosys_atoms()
 *  above. It exists primarily so tests (and any other in-process consumer)
 *  can verify attributes were captured; it is NOT exported to
 *  gnc_cognitive_export_fincosys_json(), which reads from
 *  gnc-fincosys-bridge.cpp's own, separate attribute side table.
 *
 *  @param handle Atom handle returned via a "atoms" entry during load.
 *  @param key Attribute key, e.g. "entity_code".
 *  @return The attribute value, or NULL if @a handle has no such attribute
 *          recorded.
 */
const gchar *gnc_cognitive_fincosys_loader_get_atom_attribute (GncAtomHandle handle,
                                                                 const gchar *key);

#ifdef __cplusplus
}
#endif

#endif /* GNC_COGNITIVE_FINCOSYS_LOADER_H */
