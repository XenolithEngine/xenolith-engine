/**
 Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#ifndef STAPPLER_CORE_UTILS_SPDIAGNOSTICREGISTRY_H_
#define STAPPLER_CORE_UTILS_SPDIAGNOSTICREGISTRY_H_

#include "SPString.h"

namespace STAPPLER_VERSIONIZED stappler::diagnostic {

/* Diagnostic messages addressed by NUMBER instead of carried as text.

WHAT IT IS FOR. A piece of state that has to say WHY - a control locked because something else owns
its value, a field refused for a named reason - would otherwise have to carry a String next to the
state itself. That is a heap allocation and a copy per node, for a sentence that is the same in
every instance of the same situation. Here the state carries a uint32_t and the text lives once.

STATIC MESSAGES ONLY, and this is the contract rather than a limitation to work around:

  * `registerMessage` takes a string whose storage OUTLIVES the registry - a literal. Nothing is
    copied and nothing is ever freed, so a code handed out stays valid for the life of the process;
  * consequently a message must be a CONSTANT of its module, registered once:

        static const uint32_t s_lockedByWire = diagnostic::registerMessage("the value arrives on a wire");

    and never assembled per call. A registry filled from formatted strings would grow without bound
    and would hand out a different code for what a reader sees as the same message.

WHAT THAT COSTS, said plainly: a detail that varies per instance - which wire, which node - cannot
be part of the message. Where such a detail matters it belongs beside the code, in whatever
structure already knows the instance, not in the text. */

// The code that means "nothing to say". Never returned by registerMessage.
constexpr uint32_t NoMessage = 0;

/* Register a message and get its code. The same text always yields the same code, so two modules
naming the same situation cannot end up with two numbers for it.

`text` must remain valid forever - pass a literal. An empty string yields NoMessage. */
SP_PUBLIC uint32_t registerMessage(StringView text);

// The text for a code; empty for NoMessage and for a code this process never handed out
SP_PUBLIC StringView getMessage(uint32_t code);

// How many messages are registered, excluding NoMessage. For diagnostics about the diagnostics
SP_PUBLIC uint32_t getMessageCount();

} // namespace stappler::diagnostic

#endif // STAPPLER_CORE_UTILS_SPDIAGNOSTICREGISTRY_H_
