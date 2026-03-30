/*
   ELPC.HPP
   Umbrella header for the elpc library.
   Include this to get everything, or include individual module
   headers for only what you need.
*/

#pragma once

// Core
#include <elpc/core/loc.hpp>
#include <elpc/core/table.hpp>
#include <elpc/core/token.hpp>
#include <elpc/core/tokenReader.hpp>

// Lexer
#include <elpc/lexer/lexer.hpp>
#include <elpc/lexer/rule.hpp>

// Parser
#include <elpc/parser/parser.hpp>
#include <elpc/parser/sema.hpp>

// Diagnostics
#include <elpc/diagnostics/diagnostic.hpp>
#include <elpc/diagnostics/diagnosticEngine.hpp>

// IR
#include <elpc/ir/irBuilder.hpp>
