const int DEBUG = 0;

const int NUM_SLIDERS = 3;

const int sliderPin = A0;
const int selectorBtnPin = 7;
const int muteBtnPin = 2;


int currentSlider = 0;

int analogSliderValues[NUM_SLIDERS];

/********<Debounce Stuff>********/
int selectorBtnState;             // the current reading from the input pin
int lastSelectorBtnState = LOW;   // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceSelectorTime = 0;  // the last time the output pin was toggled
unsigned long debounceSelectorDelay = 50;

int muteBtnState;             // the current reading from the input pin
int lastMuteBtnState = LOW;   // the previous reading from the input pin

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceMuteTime = 0;  // the last time the output pin was toggled
unsigned long debounceMuteDelay = 50;
/********</Debounce Stuff>********/

int mutedSlider = -1;



void setup() { 
  pinMode(sliderPin, INPUT);
  pinMode(selectorBtnPin, INPUT);
  pinMode(muteBtnPin, INPUT);

  Serial.begin(9600);

  Serial.println("Initiating...");
}

void loop() {
  if (DEBUG) {
    String str = String("Slider #") + String(currentSlider) + String(" selected");
    Serial.println(str);
  }
  updateSelectorBtn();
  updateMuteBtn();
  updateSliders();
  sendValues(); // Actually send data (all the time)
  delay(10);
}

void nextSlider() {
  if (currentSlider == (NUM_SLIDERS -1)) {
    currentSlider = 0;
  } else {
    currentSlider++;
  }
}

void muteCurrentSlider() {
   mutedSlider = currentSlider;
}


void updateSliders() {
  analogSliderValues[currentSlider] = analogRead(sliderPin);
}

void sendValues() {
  
  String builtString = String("");

  for (int i = 0; i < NUM_SLIDERS; i++) {
    if (mutedSlider != i) {
      builtString += String((int)analogSliderValues[i]);
    } else {
      builtString += String("M");
     }

    if (i < NUM_SLIDERS - 1) {
      builtString += String("|");
    }
  }

  mutedSlider = -1;
  
  Serial.println(builtString);
}

void updateSelectorBtn() {
 // read the state of the switch into a local variable:
  int reading = digitalRead(selectorBtnPin);

  // check to see if you just pressed the selectorBtn
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != lastSelectorBtnState) {
    // reset the debouncing timer
    lastDebounceSelectorTime = millis();
  }

  if ((millis() - lastDebounceSelectorTime) > debounceSelectorDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the selectorBtn state has changed:
    if (reading != selectorBtnState) {
      selectorBtnState = reading;

      // only toggle the LED if the new selectorBtn state is HIGH
      if (selectorBtnState == HIGH) {
        nextSlider();
      }
    }
 }

  // save the reading. Next time through the loop, it'll be the lastSelectorBtnState:
  lastSelectorBtnState = reading;
}

void updateMuteBtn() {
 // read the state of the switch into a local variable:
  int reading = digitalRead(muteBtnPin);

  // check to see if you just pressed the muteBtn
  // (i.e. the input went from LOW to HIGH), and you've waited long enough
  // since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != lastMuteBtnState) {
    // reset the debouncing timer
    lastDebounceMuteTime = millis();
  }

  if ((millis() - lastDebounceMuteTime) > debounceMuteDelay) {
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the muteBtn state has changed:
    if (reading != muteBtnState) {
      muteBtnState = reading;

      // only toggle the LED if the new muteBtn state is HIGH
      if (muteBtnState == HIGH) {
        muteCurrentSlider();
      }
    }
 }

  // save the reading. Next time through the loop, it'll be the lastMuteBtnState:
  lastMuteBtnState = reading;
}
