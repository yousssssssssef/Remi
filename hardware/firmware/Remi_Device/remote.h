#pragma once
#include <stdint.h>

// Phone/Mac remote: a web page served by the device (http://<device-ip>/ or http://remi.local) with a
// touch knob that sends the same events as the rotary encoder, text boxes for "add to list" and
// "ask Remi", and a setup page for Wi-Fi and the Remi account.
struct RemoteEvent {
  char kind;       // 'l' left, 'r' right, 'p' press, 'h' hold, 'a' add-to-list text, 'q' question text,
                   // 's' a phone photo arrived (text = scan mode digit; the photo waits in takePhoto())
  char text[48];
};
void remoteBegin();                     // call after backendBegin(); runs its own task
bool remoteNext(RemoteEvent &event);    // non-blocking; the UI loop drains this
