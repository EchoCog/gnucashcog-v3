/********************************************************************\
 * gnc-ko6ml-translation.cpp -- ko6ml <-> Hypergraph Translation   *
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
 * @file gnc-ko6ml-translation.cpp
 * @brief Phase 1: ko6ml Primitives & Foundational Hypergraph Encoding
 *
 * Implementation of the bidirectional translation mechanisms between
 * ko6ml primitives and AtomSpace-style hypergraph patterns, the
 * tensor fragment architecture, prime-factorization signatures, and
 * the Scheme cognitive grammar adapter.  See
 * PHASE1_KO6ML_TRANSLATION.md for the full architecture reference.
 */

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "gnc-ko6ml-translation.h"

/* ==================================================================
 * ko6ml Primitive Vocabulary
 * ================================================================== */

static const gchar *ko6ml_type_names[GNC_KO6ML_N_PRIMITIVE_TYPES] = {
    "AGENT", "STATE", "ACTION", "PERCEPT",
    "MEMORY", "GOAL", "CONSTRAINT", "RELATION"
};

const gchar* gnc_ko6ml_primitive_type_name(GncKo6mlPrimitiveType type)
{
    if (type < 0 || type >= GNC_KO6ML_N_PRIMITIVE_TYPES)
        return NULL;
    return ko6ml_type_names[type];
}

gboolean gnc_ko6ml_primitive_type_from_name(const gchar *name,
                                            GncKo6mlPrimitiveType *type)
{
    g_return_val_if_fail(name != NULL, FALSE);
    g_return_val_if_fail(type != NULL, FALSE);

    for (gint i = 0; i < GNC_KO6ML_N_PRIMITIVE_TYPES; i++) {
        if (g_strcmp0(name, ko6ml_type_names[i]) == 0) {
            *type = (GncKo6mlPrimitiveType)i;
            return TRUE;
        }
    }
    return FALSE;
}

/* ==================================================================
 * Tensor Fragment Architecture
 * ================================================================== */

gboolean gnc_ko6ml_tensor_shape_validate(const GncKo6mlTensorShape *shape,
                                         gchar **error_msg)
{
    if (error_msg)
        *error_msg = NULL;

    g_return_val_if_fail(shape != NULL, FALSE);

    struct { const gchar *name; guint value; guint max; } dims[] = {
        { "modality",       shape->modality,       GNC_KO6ML_MAX_MODALITY },
        { "depth",          shape->depth,          GNC_KO6ML_MAX_DEPTH },
        { "context",        shape->context,        GNC_KO6ML_MAX_CONTEXT },
        { "salience",       shape->salience,       GNC_KO6ML_MAX_SALIENCE },
        { "autonomy_index", shape->autonomy_index, GNC_KO6ML_MAX_AUTONOMY_INDEX }
    };

    for (gsize i = 0; i < G_N_ELEMENTS(dims); i++) {
        if (dims[i].value < 1 || dims[i].value > dims[i].max) {
            if (error_msg)
                *error_msg = g_strdup_printf(
                    "tensor dimension '%s' = %u out of range [1, %u]",
                    dims[i].name, dims[i].value, dims[i].max);
            return FALSE;
        }
    }
    return TRUE;
}

/* Prime basis of the Goedel encoding, in canonical dimension order:
 * [modality, depth, context, salience, autonomy_index] */
static const guint ko6ml_dimension_primes[GNC_KO6ML_TENSOR_DIMS] =
    { 2, 3, 5, 7, 11 };

gboolean gnc_ko6ml_tensor_shape_signature(const GncKo6mlTensorShape *shape,
                                          GncKo6mlTensorSignature *signature)
{
    g_return_val_if_fail(signature != NULL, FALSE);

    if (!gnc_ko6ml_tensor_shape_validate(shape, NULL))
        return FALSE;

    const guint exponents[GNC_KO6ML_TENSOR_DIMS] = {
        shape->modality, shape->depth, shape->context,
        shape->salience, shape->autonomy_index
    };

    /* With the canonical bounds the maximum signature is
     * 2^8 * 3^16 * 5^16 * 7^8 * 11^8 ~ 2.0e36 < 2^128, so unsigned
     * 128-bit arithmetic is exact. */
    unsigned __int128 sig = 1;
    for (guint dim = 0; dim < GNC_KO6ML_TENSOR_DIMS; dim++)
        for (guint e = 0; e < exponents[dim]; e++)
            sig *= ko6ml_dimension_primes[dim];

    signature->hi = (guint64)(sig >> 64);
    signature->lo = (guint64)(sig & G_MAXUINT64);
    return TRUE;
}

gboolean gnc_ko6ml_tensor_shape_from_signature(
    const GncKo6mlTensorSignature *signature,
    GncKo6mlTensorShape *shape)
{
    g_return_val_if_fail(signature != NULL, FALSE);
    g_return_val_if_fail(shape != NULL, FALSE);

    unsigned __int128 sig =
        (((unsigned __int128)signature->hi) << 64) | signature->lo;

    if (sig <= 1)
        return FALSE;

    guint exponents[GNC_KO6ML_TENSOR_DIMS] = { 0, 0, 0, 0, 0 };

    for (guint dim = 0; dim < GNC_KO6ML_TENSOR_DIMS; dim++) {
        const guint prime = ko6ml_dimension_primes[dim];
        while (sig % prime == 0) {
            sig /= prime;
            exponents[dim]++;
        }
    }

    /* Any residue means the signature contains prime factors outside
     * the canonical basis and cannot be a valid tensor signature */
    if (sig != 1)
        return FALSE;

    shape->modality = exponents[0];
    shape->depth = exponents[1];
    shape->context = exponents[2];
    shape->salience = exponents[3];
    shape->autonomy_index = exponents[4];

    return gnc_ko6ml_tensor_shape_validate(shape, NULL);
}

gchar* gnc_ko6ml_tensor_signature_to_string(
    const GncKo6mlTensorSignature *signature)
{
    g_return_val_if_fail(signature != NULL, NULL);

    unsigned __int128 sig =
        (((unsigned __int128)signature->hi) << 64) | signature->lo;

    if (sig == 0)
        return g_strdup("0");

    /* Render by repeated division; 128 bits needs at most 39 digits */
    gchar digits[48];
    gint pos = (gint)sizeof(digits);
    digits[--pos] = '\0';
    while (sig > 0) {
        digits[--pos] = (gchar)('0' + (int)(sig % 10));
        sig /= 10;
    }
    return g_strdup(&digits[pos]);
}

static gboolean signature_from_decimal_string(const gchar *str,
                                              GncKo6mlTensorSignature *sig_out)
{
    if (!str || !*str)
        return FALSE;

    unsigned __int128 sig = 0;
    const unsigned __int128 max = ~(unsigned __int128)0;
    for (const gchar *p = str; *p; p++) {
        if (*p < '0' || *p > '9')
            return FALSE;
        unsigned digit = (unsigned)(*p - '0');
        if (sig > (max - digit) / 10)
            return FALSE; /* overflow */
        sig = sig * 10 + digit;
    }
    sig_out->hi = (guint64)(sig >> 64);
    sig_out->lo = (guint64)(sig & G_MAXUINT64);
    return TRUE;
}

GncKo6mlTensorFragment* gnc_ko6ml_tensor_fragment_create(
    const GncKo6mlTensorShape *shape)
{
    if (!gnc_ko6ml_tensor_shape_validate(shape, NULL))
        return NULL;

    GncKo6mlTensorFragment *fragment = g_new0(GncKo6mlTensorFragment, 1);
    fragment->shape = *shape;
    fragment->element_count = (gsize)shape->modality * shape->depth *
        shape->context * shape->salience * shape->autonomy_index;
    fragment->elements = g_new0(gdouble, fragment->element_count);
    return fragment;
}

void gnc_ko6ml_tensor_fragment_free(GncKo6mlTensorFragment *fragment)
{
    if (!fragment)
        return;
    g_free(fragment->elements);
    g_free(fragment);
}

static gboolean tensor_fragment_index(const GncKo6mlTensorFragment *fragment,
                                      guint m, guint d, guint c,
                                      guint s, guint a, gsize *index)
{
    const GncKo6mlTensorShape *sh = &fragment->shape;
    if (m >= sh->modality || d >= sh->depth || c >= sh->context ||
        s >= sh->salience || a >= sh->autonomy_index)
        return FALSE;

    /* Row-major order over [modality, depth, context, salience,
     * autonomy_index] */
    *index = ((((gsize)m * sh->depth + d) * sh->context + c) *
              sh->salience + s) * sh->autonomy_index + a;
    return TRUE;
}

gboolean gnc_ko6ml_tensor_fragment_set(GncKo6mlTensorFragment *fragment,
                                       guint m, guint d, guint c,
                                       guint s, guint a, gdouble value)
{
    g_return_val_if_fail(fragment != NULL, FALSE);

    gsize index;
    if (!tensor_fragment_index(fragment, m, d, c, s, a, &index))
        return FALSE;
    fragment->elements[index] = value;
    return TRUE;
}

gboolean gnc_ko6ml_tensor_fragment_get(const GncKo6mlTensorFragment *fragment,
                                       guint m, guint d, guint c,
                                       guint s, guint a, gdouble *value)
{
    g_return_val_if_fail(fragment != NULL, FALSE);
    g_return_val_if_fail(value != NULL, FALSE);

    gsize index;
    if (!tensor_fragment_index(fragment, m, d, c, s, a, &index))
        return FALSE;
    *value = fragment->elements[index];
    return TRUE;
}

/* ==================================================================
 * ko6ml Primitive Instances
 * ================================================================== */

GncKo6mlPrimitive* gnc_ko6ml_primitive_create(GncKo6mlPrimitiveType type,
                                              const gchar *name,
                                              const GncKo6mlTensorShape *shape,
                                              gdouble strength,
                                              gdouble confidence)
{
    g_return_val_if_fail(name != NULL && *name != '\0', NULL);
    g_return_val_if_fail(strength >= 0.0 && strength <= 1.0, NULL);
    g_return_val_if_fail(confidence >= 0.0 && confidence <= 1.0, NULL);

    if (gnc_ko6ml_primitive_type_name(type) == NULL)
        return NULL;
    if (!gnc_ko6ml_tensor_shape_validate(shape, NULL))
        return NULL;

    GncKo6mlPrimitive *primitive = g_new0(GncKo6mlPrimitive, 1);
    primitive->type = type;
    primitive->name = g_strdup(name);
    primitive->shape = *shape;
    primitive->strength = strength;
    primitive->confidence = confidence;
    primitive->properties = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, g_free);
    return primitive;
}

void gnc_ko6ml_primitive_free(GncKo6mlPrimitive *primitive)
{
    if (!primitive)
        return;
    g_free(primitive->name);
    if (primitive->properties)
        g_hash_table_destroy(primitive->properties);
    g_free(primitive);
}

void gnc_ko6ml_primitive_set_property(GncKo6mlPrimitive *primitive,
                                      const gchar *key,
                                      const gchar *value)
{
    g_return_if_fail(primitive != NULL);
    g_return_if_fail(key != NULL && *key != '\0');
    g_return_if_fail(value != NULL);

    g_hash_table_replace(primitive->properties, g_strdup(key),
                         g_strdup(value));
}

const gchar* gnc_ko6ml_primitive_get_property(
    const GncKo6mlPrimitive *primitive, const gchar *key)
{
    g_return_val_if_fail(primitive != NULL, NULL);
    g_return_val_if_fail(key != NULL, NULL);

    return (const gchar*)g_hash_table_lookup(primitive->properties, key);
}

gboolean gnc_ko6ml_primitive_equal(const GncKo6mlPrimitive *a,
                                   const GncKo6mlPrimitive *b)
{
    if (a == b)
        return TRUE;
    if (!a || !b)
        return FALSE;

    if (a->type != b->type)
        return FALSE;
    if (g_strcmp0(a->name, b->name) != 0)
        return FALSE;
    if (memcmp(&a->shape, &b->shape, sizeof(GncKo6mlTensorShape)) != 0)
        return FALSE;
    if (fabs(a->strength - b->strength) > 1e-9)
        return FALSE;
    if (fabs(a->confidence - b->confidence) > 1e-9)
        return FALSE;

    if (g_hash_table_size(a->properties) != g_hash_table_size(b->properties))
        return FALSE;

    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, a->properties);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const gchar *other =
            (const gchar*)g_hash_table_lookup(b->properties, key);
        if (g_strcmp0((const gchar*)value, other) != 0)
            return FALSE;
    }
    return TRUE;
}

/* ==================================================================
 * Hypergraph Fragment Encoding
 * ================================================================== */

static GncKo6mlHypergraphAtom* hypergraph_atom_new(GncKo6mlAtomType atom_type,
                                                   const gchar *name,
                                                   gdouble strength,
                                                   gdouble confidence)
{
    GncKo6mlHypergraphAtom *atom = g_new0(GncKo6mlHypergraphAtom, 1);
    atom->atom_type = atom_type;
    atom->name = g_strdup(name);
    atom->strength = strength;
    atom->confidence = confidence;
    return atom;
}

static void hypergraph_atom_free(gpointer data)
{
    GncKo6mlHypergraphAtom *atom = (GncKo6mlHypergraphAtom*)data;
    if (!atom)
        return;
    g_free(atom->name);
    g_free(atom->outgoing);
    g_free(atom);
}

void gnc_ko6ml_hypergraph_fragment_free(GncKo6mlHypergraphFragment *fragment)
{
    if (!fragment)
        return;
    if (fragment->atoms)
        g_ptr_array_free(fragment->atoms, TRUE);
    g_free(fragment);
}

static guint fragment_add_atom(GncKo6mlHypergraphFragment *fragment,
                               GncKo6mlHypergraphAtom *atom)
{
    g_ptr_array_add(fragment->atoms, atom);
    return fragment->atoms->len - 1;
}

static guint fragment_add_link(GncKo6mlHypergraphFragment *fragment,
                               GncKo6mlAtomType link_type,
                               const gchar *label,
                               guint source, guint target,
                               gdouble strength, gdouble confidence)
{
    GncKo6mlHypergraphAtom *link =
        hypergraph_atom_new(link_type, label, strength, confidence);
    link->outgoing = g_new(guint, 2);
    link->outgoing[0] = source;
    link->outgoing[1] = target;
    link->outgoing_count = 2;
    return fragment_add_atom(fragment, link);
}

GncKo6mlHypergraphFragment* gnc_ko6ml_primitive_to_hypergraph(
    const GncKo6mlPrimitive *primitive)
{
    g_return_val_if_fail(primitive != NULL, NULL);

    const gchar *type_name = gnc_ko6ml_primitive_type_name(primitive->type);
    if (!type_name)
        return NULL;

    GncKo6mlTensorSignature signature;
    if (!gnc_ko6ml_tensor_shape_signature(&primitive->shape, &signature))
        return NULL;

    GncKo6mlHypergraphFragment *fragment =
        g_new0(GncKo6mlHypergraphFragment, 1);
    fragment->atoms = g_ptr_array_new_with_free_func(hypergraph_atom_free);

    /* Root concept: ConceptNode "Ko6ml:<TYPE>:<name>" carries the
     * primitive's truth value */
    gchar *root_name = g_strdup_printf("Ko6ml:%s:%s", type_name,
                                       primitive->name);
    guint root = fragment_add_atom(fragment,
        hypergraph_atom_new(GNC_KO6ML_NODE_CONCEPT, root_name,
                            primitive->strength, primitive->confidence));
    g_free(root_name);

    /* Type membership: InheritanceLink root -> "Ko6mlType:<TYPE>" */
    gchar *type_concept_name = g_strdup_printf("Ko6mlType:%s", type_name);
    guint type_concept = fragment_add_atom(fragment,
        hypergraph_atom_new(GNC_KO6ML_NODE_CONCEPT, type_concept_name,
                            1.0, 1.0));
    g_free(type_concept_name);
    fragment_add_link(fragment, GNC_KO6ML_LINK_INHERITANCE,
                      "ko6ml-type", root, type_concept, 1.0, 1.0);

    /* Tensor shape: EvaluationLink hasTensorShape ->
     * NumberNode of the prime-factorization signature */
    guint shape_predicate = fragment_add_atom(fragment,
        hypergraph_atom_new(GNC_KO6ML_NODE_PREDICATE, "hasTensorShape",
                            1.0, 1.0));
    gchar *sig_str = gnc_ko6ml_tensor_signature_to_string(&signature);
    guint shape_number = fragment_add_atom(fragment,
        hypergraph_atom_new(GNC_KO6ML_NODE_NUMBER, sig_str, 1.0, 1.0));
    g_free(sig_str);
    fragment_add_link(fragment, GNC_KO6ML_LINK_EVALUATION,
                      "hasTensorShape", shape_predicate, shape_number,
                      1.0, 1.0);

    /* Properties: EvaluationLink hasProperty:<key> ->
     * ConceptNode of the value, one per property, in sorted key order
     * for deterministic encoding */
    GList *keys = g_hash_table_get_keys(primitive->properties);
    keys = g_list_sort(keys, (GCompareFunc)g_strcmp0);
    for (GList *node = keys; node; node = node->next) {
        const gchar *key = (const gchar*)node->data;
        const gchar *value = (const gchar*)g_hash_table_lookup(
            primitive->properties, key);

        gchar *predicate_name = g_strdup_printf("hasProperty:%s", key);
        guint prop_predicate = fragment_add_atom(fragment,
            hypergraph_atom_new(GNC_KO6ML_NODE_PREDICATE, predicate_name,
                                1.0, 1.0));
        g_free(predicate_name);

        guint prop_value = fragment_add_atom(fragment,
            hypergraph_atom_new(GNC_KO6ML_NODE_CONCEPT, value, 1.0, 1.0));
        fragment_add_link(fragment, GNC_KO6ML_LINK_EVALUATION,
                          "hasProperty", prop_predicate, prop_value,
                          1.0, 1.0);
    }
    g_list_free(keys);

    return fragment;
}

static GncKo6mlHypergraphAtom* fragment_atom(
    const GncKo6mlHypergraphFragment *fragment, guint index)
{
    if (index >= fragment->atoms->len)
        return NULL;
    return (GncKo6mlHypergraphAtom*)g_ptr_array_index(fragment->atoms, index);
}

GncKo6mlPrimitive* gnc_ko6ml_hypergraph_to_primitive(
    const GncKo6mlHypergraphFragment *fragment)
{
    g_return_val_if_fail(fragment != NULL, NULL);
    g_return_val_if_fail(fragment->atoms != NULL, NULL);

    /* Locate the root concept: the ConceptNode named "Ko6ml:..." */
    GncKo6mlHypergraphAtom *root = NULL;
    guint root_index = 0;
    for (guint i = 0; i < fragment->atoms->len; i++) {
        GncKo6mlHypergraphAtom *atom = fragment_atom(fragment, i);
        if (atom->atom_type == GNC_KO6ML_NODE_CONCEPT &&
            g_str_has_prefix(atom->name, "Ko6ml:")) {
            root = atom;
            root_index = i;
            break;
        }
    }
    if (!root)
        return NULL;

    /* Parse "Ko6ml:<TYPE>:<name>" -- the name may itself contain
     * colons, so split at the second separator only */
    const gchar *type_start = root->name + strlen("Ko6ml:");
    const gchar *name_sep = strchr(type_start, ':');
    if (!name_sep || *(name_sep + 1) == '\0')
        return NULL;

    gchar *type_name = g_strndup(type_start, (gsize)(name_sep - type_start));
    GncKo6mlPrimitiveType type;
    gboolean type_ok = gnc_ko6ml_primitive_type_from_name(type_name, &type);
    g_free(type_name);
    if (!type_ok)
        return NULL;

    const gchar *primitive_name = name_sep + 1;

    /* Verify the type membership InheritanceLink for structural
     * integrity of the fragment */
    gboolean inheritance_ok = FALSE;
    for (guint i = 0; i < fragment->atoms->len; i++) {
        GncKo6mlHypergraphAtom *atom = fragment_atom(fragment, i);
        if (atom->atom_type != GNC_KO6ML_LINK_INHERITANCE ||
            atom->outgoing_count != 2 ||
            atom->outgoing[0] != root_index)
            continue;
        GncKo6mlHypergraphAtom *target =
            fragment_atom(fragment, atom->outgoing[1]);
        if (target && g_str_has_prefix(target->name, "Ko6mlType:")) {
            inheritance_ok = TRUE;
            break;
        }
    }
    if (!inheritance_ok)
        return NULL;

    /* Recover the tensor shape from the hasTensorShape evaluation */
    GncKo6mlTensorShape shape;
    gboolean shape_found = FALSE;
    for (guint i = 0; i < fragment->atoms->len && !shape_found; i++) {
        GncKo6mlHypergraphAtom *atom = fragment_atom(fragment, i);
        if (atom->atom_type != GNC_KO6ML_LINK_EVALUATION ||
            atom->outgoing_count != 2)
            continue;
        GncKo6mlHypergraphAtom *predicate =
            fragment_atom(fragment, atom->outgoing[0]);
        GncKo6mlHypergraphAtom *number =
            fragment_atom(fragment, atom->outgoing[1]);
        if (!predicate || !number ||
            predicate->atom_type != GNC_KO6ML_NODE_PREDICATE ||
            g_strcmp0(predicate->name, "hasTensorShape") != 0 ||
            number->atom_type != GNC_KO6ML_NODE_NUMBER)
            continue;

        GncKo6mlTensorSignature signature;
        if (!signature_from_decimal_string(number->name, &signature))
            return NULL;
        if (!gnc_ko6ml_tensor_shape_from_signature(&signature, &shape))
            return NULL;
        shape_found = TRUE;
    }
    if (!shape_found)
        return NULL;

    GncKo6mlPrimitive *primitive = gnc_ko6ml_primitive_create(
        type, primitive_name, &shape, root->strength, root->confidence);
    if (!primitive)
        return NULL;

    /* Recover properties from hasProperty:<key> evaluations */
    for (guint i = 0; i < fragment->atoms->len; i++) {
        GncKo6mlHypergraphAtom *atom = fragment_atom(fragment, i);
        if (atom->atom_type != GNC_KO6ML_LINK_EVALUATION ||
            atom->outgoing_count != 2)
            continue;
        GncKo6mlHypergraphAtom *predicate =
            fragment_atom(fragment, atom->outgoing[0]);
        GncKo6mlHypergraphAtom *value =
            fragment_atom(fragment, atom->outgoing[1]);
        if (!predicate || !value ||
            predicate->atom_type != GNC_KO6ML_NODE_PREDICATE ||
            !g_str_has_prefix(predicate->name, "hasProperty:") ||
            value->atom_type != GNC_KO6ML_NODE_CONCEPT)
            continue;

        const gchar *key = predicate->name + strlen("hasProperty:");
        gnc_ko6ml_primitive_set_property(primitive, key, value->name);
    }

    return primitive;
}

gboolean gnc_ko6ml_verify_hypergraph_roundtrip(
    const GncKo6mlPrimitive *primitive)
{
    g_return_val_if_fail(primitive != NULL, FALSE);

    GncKo6mlHypergraphFragment *fragment =
        gnc_ko6ml_primitive_to_hypergraph(primitive);
    if (!fragment)
        return FALSE;

    GncKo6mlPrimitive *recovered =
        gnc_ko6ml_hypergraph_to_primitive(fragment);
    gnc_ko6ml_hypergraph_fragment_free(fragment);
    if (!recovered)
        return FALSE;

    gboolean equal = gnc_ko6ml_primitive_equal(primitive, recovered);
    gnc_ko6ml_primitive_free(recovered);
    return equal;
}

/* ==================================================================
 * Scheme Cognitive Grammar Adapter
 * ================================================================== */

/* Escape a string for embedding in a Scheme string literal */
static gchar* scheme_escape_string(const gchar *str)
{
    GString *out = g_string_new("");
    for (const gchar *p = str; *p; p++) {
        if (*p == '"' || *p == '\\')
            g_string_append_c(out, '\\');
        g_string_append_c(out, *p);
    }
    return g_string_free(out, FALSE);
}

gchar* gnc_ko6ml_primitive_to_scheme(const GncKo6mlPrimitive *primitive)
{
    g_return_val_if_fail(primitive != NULL, NULL);

    const gchar *type_name = gnc_ko6ml_primitive_type_name(primitive->type);
    if (!type_name)
        return NULL;

    GString *expr = g_string_new("(Ko6mlPrimitive");

    g_string_append_printf(expr, " (type \"%s\")", type_name);

    gchar *escaped_name = scheme_escape_string(primitive->name);
    g_string_append_printf(expr, " (name \"%s\")", escaped_name);
    g_free(escaped_name);

    g_string_append_printf(expr,
        " (tensor-shape (modality %u) (depth %u) (context %u)"
        " (salience %u) (autonomy-index %u))",
        primitive->shape.modality, primitive->shape.depth,
        primitive->shape.context, primitive->shape.salience,
        primitive->shape.autonomy_index);

    gchar strength_buf[G_ASCII_DTOSTR_BUF_SIZE];
    gchar confidence_buf[G_ASCII_DTOSTR_BUF_SIZE];
    g_ascii_formatd(strength_buf, sizeof(strength_buf), "%.17g",
                    primitive->strength);
    g_ascii_formatd(confidence_buf, sizeof(confidence_buf), "%.17g",
                    primitive->confidence);
    g_string_append_printf(expr,
        " (truth (strength %s) (confidence %s))",
        strength_buf, confidence_buf);

    g_string_append(expr, " (properties");
    GList *keys = g_hash_table_get_keys(primitive->properties);
    keys = g_list_sort(keys, (GCompareFunc)g_strcmp0);
    for (GList *node = keys; node; node = node->next) {
        const gchar *key = (const gchar*)node->data;
        const gchar *value = (const gchar*)g_hash_table_lookup(
            primitive->properties, key);
        gchar *escaped_key = scheme_escape_string(key);
        gchar *escaped_value = scheme_escape_string(value);
        g_string_append_printf(expr, " (prop \"%s\" \"%s\")",
                               escaped_key, escaped_value);
        g_free(escaped_key);
        g_free(escaped_value);
    }
    g_list_free(keys);
    g_string_append(expr, ")");

    g_string_append(expr, ")");
    return g_string_free(expr, FALSE);
}

/* --- Minimal recursive-descent s-expression parser --------------- */

typedef struct SExpr SExpr;
struct SExpr {
    gboolean is_list;   /* TRUE: list of children, FALSE: atom token */
    gchar *atom;        /* Atom text (unquoted/unescaped for strings) */
    GPtrArray *children;/* Child SExpr* when is_list */
};

static void sexpr_free(SExpr *expr)
{
    if (!expr)
        return;
    g_free(expr->atom);
    if (expr->children) {
        for (guint i = 0; i < expr->children->len; i++)
            sexpr_free((SExpr*)g_ptr_array_index(expr->children, i));
        g_ptr_array_free(expr->children, TRUE);
    }
    g_free(expr);
}

static void skip_whitespace(const gchar **p)
{
    while (**p && g_ascii_isspace(**p))
        (*p)++;
}

static SExpr* parse_sexpr(const gchar **p);

static SExpr* parse_atom(const gchar **p)
{
    SExpr *expr = g_new0(SExpr, 1);
    expr->is_list = FALSE;

    if (**p == '"') {
        /* Quoted string with backslash escapes */
        (*p)++;
        GString *out = g_string_new("");
        while (**p && **p != '"') {
            if (**p == '\\' && *(*p + 1)) {
                (*p)++;
            }
            g_string_append_c(out, **p);
            (*p)++;
        }
        if (**p != '"') {
            g_string_free(out, TRUE);
            g_free(expr);
            return NULL; /* unterminated string */
        }
        (*p)++;
        expr->atom = g_string_free(out, FALSE);
    } else {
        const gchar *start = *p;
        while (**p && !g_ascii_isspace(**p) && **p != '(' && **p != ')')
            (*p)++;
        if (*p == start) {
            g_free(expr);
            return NULL;
        }
        expr->atom = g_strndup(start, (gsize)(*p - start));
    }
    return expr;
}

static SExpr* parse_sexpr(const gchar **p)
{
    skip_whitespace(p);

    if (**p == '(') {
        (*p)++;
        SExpr *expr = g_new0(SExpr, 1);
        expr->is_list = TRUE;
        expr->children = g_ptr_array_new();

        for (;;) {
            skip_whitespace(p);
            if (**p == ')') {
                (*p)++;
                return expr;
            }
            if (**p == '\0') {
                sexpr_free(expr);
                return NULL; /* unterminated list */
            }
            SExpr *child = parse_sexpr(p);
            if (!child) {
                sexpr_free(expr);
                return NULL;
            }
            g_ptr_array_add(expr->children, child);
        }
    }

    if (**p == ')' || **p == '\0')
        return NULL;

    return parse_atom(p);
}

/* Find the child list whose head atom equals `tag` */
static SExpr* sexpr_find_tagged(SExpr *list, const gchar *tag)
{
    if (!list || !list->is_list)
        return NULL;
    for (guint i = 0; i < list->children->len; i++) {
        SExpr *child = (SExpr*)g_ptr_array_index(list->children, i);
        if (child->is_list && child->children->len > 0) {
            SExpr *head = (SExpr*)g_ptr_array_index(child->children, 0);
            if (!head->is_list && g_strcmp0(head->atom, tag) == 0)
                return child;
        }
    }
    return NULL;
}

/* Return the atom text of child at index, or NULL */
static const gchar* sexpr_atom_at(SExpr *list, guint index)
{
    if (!list || !list->is_list || index >= list->children->len)
        return NULL;
    SExpr *child = (SExpr*)g_ptr_array_index(list->children, index);
    return child->is_list ? NULL : child->atom;
}

static gboolean sexpr_uint_field(SExpr *list, const gchar *tag, guint *out)
{
    SExpr *field = sexpr_find_tagged(list, tag);
    const gchar *text = sexpr_atom_at(field, 1);
    if (!text)
        return FALSE;
    gchar *end = NULL;
    guint64 value = g_ascii_strtoull(text, &end, 10);
    if (!end || *end != '\0' || value > G_MAXUINT)
        return FALSE;
    *out = (guint)value;
    return TRUE;
}

static gboolean sexpr_double_field(SExpr *list, const gchar *tag,
                                   gdouble *out)
{
    SExpr *field = sexpr_find_tagged(list, tag);
    const gchar *text = sexpr_atom_at(field, 1);
    if (!text)
        return FALSE;
    gchar *end = NULL;
    gdouble value = g_ascii_strtod(text, &end);
    if (!end || *end != '\0')
        return FALSE;
    *out = value;
    return TRUE;
}

GncKo6mlPrimitive* gnc_ko6ml_primitive_from_scheme(const gchar *scheme_expr)
{
    g_return_val_if_fail(scheme_expr != NULL, NULL);

    const gchar *cursor = scheme_expr;
    SExpr *root = parse_sexpr(&cursor);
    if (!root)
        return NULL;

    GncKo6mlPrimitive *primitive = NULL;

    /* Validate document structure: (Ko6mlPrimitive ...) */
    do {
        if (!root->is_list || root->children->len < 1)
            break;
        const gchar *head = sexpr_atom_at(root, 0);
        if (g_strcmp0(head, "Ko6mlPrimitive") != 0)
            break;

        const gchar *type_name =
            sexpr_atom_at(sexpr_find_tagged(root, "type"), 1);
        const gchar *name =
            sexpr_atom_at(sexpr_find_tagged(root, "name"), 1);
        if (!type_name || !name)
            break;

        GncKo6mlPrimitiveType type;
        if (!gnc_ko6ml_primitive_type_from_name(type_name, &type))
            break;

        SExpr *shape_expr = sexpr_find_tagged(root, "tensor-shape");
        GncKo6mlTensorShape shape;
        if (!sexpr_uint_field(shape_expr, "modality", &shape.modality) ||
            !sexpr_uint_field(shape_expr, "depth", &shape.depth) ||
            !sexpr_uint_field(shape_expr, "context", &shape.context) ||
            !sexpr_uint_field(shape_expr, "salience", &shape.salience) ||
            !sexpr_uint_field(shape_expr, "autonomy-index",
                              &shape.autonomy_index))
            break;

        SExpr *truth_expr = sexpr_find_tagged(root, "truth");
        gdouble strength, confidence;
        if (!sexpr_double_field(truth_expr, "strength", &strength) ||
            !sexpr_double_field(truth_expr, "confidence", &confidence))
            break;

        primitive = gnc_ko6ml_primitive_create(type, name, &shape,
                                               strength, confidence);
        if (!primitive)
            break;

        SExpr *props = sexpr_find_tagged(root, "properties");
        if (props) {
            for (guint i = 1; i < props->children->len; i++) {
                SExpr *prop =
                    (SExpr*)g_ptr_array_index(props->children, i);
                if (!prop->is_list || prop->children->len != 3)
                    continue;
                const gchar *tag = sexpr_atom_at(prop, 0);
                const gchar *key = sexpr_atom_at(prop, 1);
                const gchar *value = sexpr_atom_at(prop, 2);
                if (g_strcmp0(tag, "prop") == 0 && key && *key && value)
                    gnc_ko6ml_primitive_set_property(primitive, key, value);
            }
        }
    } while (FALSE);

    sexpr_free(root);
    return primitive;
}

gboolean gnc_ko6ml_verify_scheme_roundtrip(const GncKo6mlPrimitive *primitive)
{
    g_return_val_if_fail(primitive != NULL, FALSE);

    gchar *scheme_expr = gnc_ko6ml_primitive_to_scheme(primitive);
    if (!scheme_expr)
        return FALSE;

    GncKo6mlPrimitive *recovered =
        gnc_ko6ml_primitive_from_scheme(scheme_expr);
    g_free(scheme_expr);
    if (!recovered)
        return FALSE;

    gboolean equal = gnc_ko6ml_primitive_equal(primitive, recovered);
    gnc_ko6ml_primitive_free(recovered);
    return equal;
}

/* ==================================================================
 * Verification Protocol
 * ================================================================== */

gboolean gnc_ko6ml_benchmark_roundtrip(guint iterations,
                                       GncKo6mlBenchmarkResult *result)
{
    g_return_val_if_fail(iterations >= 1, FALSE);
    g_return_val_if_fail(result != NULL, FALSE);

    result->iterations = 0;
    result->hypergraph_us_per_op = 0.0;
    result->scheme_us_per_op = 0.0;
    result->all_roundtrips_ok = TRUE;

    /* One representative primitive per type, with distinct shapes and
     * properties -- exercised for real (no simulation) */
    std::vector<GncKo6mlPrimitive*> primitives;
    for (gint t = 0; t < GNC_KO6ML_N_PRIMITIVE_TYPES; t++) {
        GncKo6mlTensorShape shape = {
            (guint)(1 + t % GNC_KO6ML_MAX_MODALITY),
            (guint)(1 + t % GNC_KO6ML_MAX_DEPTH),
            (guint)(1 + (t * 2) % GNC_KO6ML_MAX_CONTEXT),
            (guint)(1 + t % GNC_KO6ML_MAX_SALIENCE),
            (guint)(1 + t % GNC_KO6ML_MAX_AUTONOMY_INDEX)
        };
        gchar *name = g_strdup_printf("bench-%s",
            gnc_ko6ml_primitive_type_name((GncKo6mlPrimitiveType)t));
        GncKo6mlPrimitive *primitive = gnc_ko6ml_primitive_create(
            (GncKo6mlPrimitiveType)t, name, &shape,
            0.5 + 0.05 * t, 0.9 - 0.05 * t);
        g_free(name);
        if (!primitive) {
            for (GncKo6mlPrimitive *p : primitives)
                gnc_ko6ml_primitive_free(p);
            return FALSE;
        }
        gnc_ko6ml_primitive_set_property(primitive, "phase", "1");
        gnc_ko6ml_primitive_set_property(primitive, "origin", "benchmark");
        primitives.push_back(primitive);
    }

    guint total_ops = 0;

    gint64 hypergraph_start = g_get_monotonic_time();
    for (guint i = 0; i < iterations; i++) {
        for (GncKo6mlPrimitive *primitive : primitives) {
            if (!gnc_ko6ml_verify_hypergraph_roundtrip(primitive))
                result->all_roundtrips_ok = FALSE;
            total_ops++;
        }
    }
    gint64 hypergraph_elapsed = g_get_monotonic_time() - hypergraph_start;

    gint64 scheme_start = g_get_monotonic_time();
    for (guint i = 0; i < iterations; i++) {
        for (GncKo6mlPrimitive *primitive : primitives) {
            if (!gnc_ko6ml_verify_scheme_roundtrip(primitive))
                result->all_roundtrips_ok = FALSE;
        }
    }
    gint64 scheme_elapsed = g_get_monotonic_time() - scheme_start;

    result->iterations = total_ops;
    result->hypergraph_us_per_op =
        (gdouble)hypergraph_elapsed / (gdouble)total_ops;
    result->scheme_us_per_op =
        (gdouble)scheme_elapsed / (gdouble)total_ops;

    for (GncKo6mlPrimitive *primitive : primitives)
        gnc_ko6ml_primitive_free(primitive);

    return result->all_roundtrips_ok;
}
