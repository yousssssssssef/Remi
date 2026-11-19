#pragma once
// Remi device UI: boot intro, then four pages (Home, Just bought, Cook, Talk) driven by a rotary encoder.
// Portable: renders into gfx::fb; the platform pushes the buffer to the screen.

namespace ui {

enum class Input { Left, Right, Press, LongPress };

void begin();               // starts the boot intro
void input(Input in);       // encoder/button events
void render();              // draws the current frame into gfx::fb
bool inIntro();
void toggleColorTest();            // background swatches for picking a shade on the real panel
void notify(const char *msg);      // short message at the bottom of the screen (shown after the boot)
// Text typed on the phone/Mac remote: "add milk" searches the catalogue for the shopping list,
// anything else is a question for Remi.
enum class TextKind { Add, Ask };
void textInput(TextKind kind, const char *text);
void photoArrived(int scanMode);  // a photo from the phone remote is being read: show the Scan flow

}  // namespace ui
