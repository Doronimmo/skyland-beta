#pragma once

#include "number/numeric.hpp"



namespace skyland {



// A globaly unique id, for uniquely identifying characters. I tried to get away
// without assigning ids to characters, but after dealing with bugs in obscure
// cases, I determined that characters need to be uniquely
// identifiable. Especially for rewinding history. Let's say that you want to
// rewind history faster than the original game speed. A character might be
// injured while walking, but when the game is rewound at double speed, the
// character might not actually be standing in the same position as it was when
// it was injured, so we need to use something other than a character's tile
// position to uniquely identify a character.
using CharacterId = u16;



} // namespace skyland
