/********************************************************************\
 * gnc-cognitive-json-util.h -- Shared JSON string escaping helper   *
 *                                                                    *
 * This program is free software; you can redistribute it and/or     *
 * modify it under the terms of the GNU General Public License as    *
 * published by the Free Software Foundation; either version 2 of    *
 * the License, or (at your option) any later version.               *
 *                                                                    *
 * This program is distributed in the hope that it will be useful,   *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the     *
 * GNU General Public License for more details.                      *
 *                                                                    *
\********************************************************************/

/** @file gnc-cognitive-json-util.h
    @brief Escape a string for embedding as a JSON string literal.

    Shared by the no-json-glib fallbacks in gnc-cognitive-api.c and
    gnc-cognitive-graphql.c, which build JSON with g_strdup_printf()
    rather than JsonBuilder (which escapes automatically) -- without
    this, a quote or backslash in an interpolated value (e.g. a
    path-derived agent/account id, or an error message) would break
    the JSON payload or inject fields.
*/

#ifndef GNC_COGNITIVE_JSON_UTIL_H
#define GNC_COGNITIVE_JSON_UTIL_H

#include <glib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline gchar* gnc_cognitive_json_escape_string(const gchar *s)
{
    if (!s)
        return g_strdup("");

    GString *out = g_string_sized_new(strlen(s));
    for (const gchar *p = s; *p; p++) {
        switch (*p) {
            case '"':  g_string_append(out, "\\\""); break;
            case '\\': g_string_append(out, "\\\\"); break;
            case '\n': g_string_append(out, "\\n"); break;
            case '\r': g_string_append(out, "\\r"); break;
            case '\t': g_string_append(out, "\\t"); break;
            default:
                if ((guchar)*p < 0x20)
                    g_string_append_printf(out, "\\u%04x", (guchar)*p);
                else
                    g_string_append_c(out, *p);
        }
    }
    return g_string_free(out, FALSE);
}

#ifdef __cplusplus
}
#endif

#endif /* GNC_COGNITIVE_JSON_UTIL_H */
