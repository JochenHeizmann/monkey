

// Set to 0 to disable 'hi-res' mode on certain devices.
//
#define RETINA_ENABLED 1

// Set to 0 to disable accelerometer - may give more CPU grunt?
//
#define ACCELEROMETER_ENABLED 1

// Set to 1 to enable use of 'displaylink' timing.
// This will only apply for apps that use SetUpdateRate 60 - different hertz will use normal NSTimer.
// This should give more accurate 60hz timing, ie: smoother updates, but is also reputed to cause laggy input so is
// off by default.
//
#define DISPLAY_LINK_ENABLED 0
