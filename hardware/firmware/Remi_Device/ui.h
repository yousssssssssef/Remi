#pragma once
// Remi device UI: boot intro, then four pages (Home, Just bought, Cook, Talk) driven by a rotary encoder.
// Portable: renders into gfx::fb; the platform pushes the buffer to the screen.

namespace ui {

enum class Input { Left, Right, Press, LongPress };

void begin();               // starts the boot intro
void input(Input in);       // encoder/button events
void render();              // draws the current frame into gfx::fb
bool inIntro();
void toggleColorTest();  // background swatches for picking a shade on the real panel

}  // namespace ui
