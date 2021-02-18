const int NUM_SLIDERS = 5;
const int sliderPin = A0;
const int buttonPin = 2;

const int DEBUG = 1;

int currentSlider = 0;
int buttonState;             // the current reading from the input pin
int lastButtonState = LOW;   // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;

int analogSliderValues[NUM_SLIDERS];
int initialValue = 600;

void setup() { 
  pinMode(sliderPin, INPUT);
  pinMode(buttonPin, INPUT);

  for (int i = 1; i < NUM_SLIDERS; i++) {
   analogSliderValues[i] =initialValue;
  }

  Serial.begin(9600);
}

void loop() {
  if (DEBUG) {
    String str = String("Slider #") + String(currentSlider) + String(" selected");
    Serial.println(str);
  }
  updateButton();
  updateSliders();
  sendSliderValues(); // Actually send data (all the time)
  delay(10);
}

void nextSlider() {
  if (currentSlider == (NUM_SLIDERS -1)) {
    currentSlider = 0;
  } else {
    currentSlider++;
  }
}

void updateButton() {
 // read the state of the switch into a local variable:
  int reading = digitalRead(buttonPin);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LED if the new button state is HIGH
      if (buttonState == HIGH) {
        nextSlider();
      }
    }
 }

  // save the reading. Next time through the loop, it'll be the lastButtonState:
  lastButtonState = reading;
}

void updateSliders() {
  analogSliderValues[currentSlider] = analogRead(sliderPin);

}

void sendSliderValues() {
  
  String builtString = String("");

  for (int i = 0; i < NUM_SLIDERS; i++) {
    builtString += String((int)analogSliderValues[i]);

    if (i < NUM_SLIDERS - 1) {
      builtString += String("|");
    }
  }
  
  Serial.println(builtString);
}
