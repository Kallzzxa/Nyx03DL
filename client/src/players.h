// players.h — hand-rolled minimal JSON parser for GET /nyx03dl/players.
//
// Written for Nyx03DL (M3). No external JSON library (same reason as
// manifest.*): the schema is fixed and tiny.
//
// Players schema (from the M1 plugin, GET /nyx03dl/players):
//   {"0": 20000, "5": 20001}          — playerid (as a JSON string key)
//                                       maps to the custom skin that
//                                       player currently wears.
//   {}                                — nobody wears a custom skin.
//
// Keys are decimal player ids (0..999); values are custom skin ids
// (20000..30000). Malformed JSON is rejected with an error string — the
// parser never crashes on bad input, and the caller (monitor) keeps the
// previous mapping on failure instead of trusting a partial parse.
#pragma once

#include <map>
#include <string>

namespace nyx {

// Parse a /players JSON document. Returns false (and fills `err`) on any
// malformed input. `out` is left untouched on failure and fully replaced
// on success.
bool ParsePlayers(const std::string& json, std::map<int, int>& out, std::string& err);

}  // namespace nyx
