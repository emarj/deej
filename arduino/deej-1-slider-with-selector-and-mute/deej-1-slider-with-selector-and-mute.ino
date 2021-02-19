const int DEBUG = 1;

const int NUM_SLIDERS = 3;
const int CMD_SIZE = 6;
const int LINE_BUFFER = CMD_SIZE * NUM_SLIDERS + (NUM_SLIDERS -1) +1;

const int sliderPin = A0;
const int selectorBtnPin = 7;
const int muteBtnPin = 2;
const int ledPin = 13;


int selectedSlider = 0;

int analogSliderValues[NUM_SLIDERS];
int analogSliderMute[NUM_SLIDERS];

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

int sent = 0;


void setup() {
  pinMode(sliderPin, INPUT);
  pinMode(selectorBtnPin, INPUT);
  pinMode(muteBtnPin, INPUT);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  Serial.begin(9600);

  Serial.println("Initiating...");


}

void loop() {

  updateSelectorBtn();
  updateMuteBtn();
  updateSliders();

  updateUI();
  sendValues(); // Actually send data (all the time)
  readValues();

  delay(10);
}

void nextSlider() {
  if (selectedSlider == (NUM_SLIDERS - 1)) {
    selectedSlider = 0;
  } else {
    selectedSlider++;
  }
   if (DEBUG) {
    String str = String("Slider #") + String(selectedSlider) + String(" selected");
    Serial.println(str);
  }
}

void muteCurrentSlider() {
  analogSliderMute[selectedSlider] = !analogSliderMute[selectedSlider];
}


void updateSliders() {
  analogSliderValues[selectedSlider] = analogRead(sliderPin);
}

void sendValues() {

  String builtString = String("");

  for (int i = 0; i < NUM_SLIDERS; i++) {

    builtString += String((int)analogSliderValues[i]);

    builtString += String(":") + String((analogSliderMute[i]) ? "M" : "U");

    if (i < NUM_SLIDERS - 1) {
      builtString += String("|");
    }


  }

  Serial.println(builtString);
}

void readValues() {
  char input[LINE_BUFFER];
  char buffer[30];

  if (!Serial.available()) {
    return;
    }
    
  byte size = Serial.readBytes(input, LINE_BUFFER);
  // Add the final 0 to end the C string
  input[size] = '\0';
  int sliderID = 0;
  

  // Read each command pair
  char* command = strtok(input, "|");
  while (command != 0)
  {
    // Split the command in two values
    char* separator = strchr(command, ':');
    if (separator != 0)
    {
      // Actually split the string in 2: replace ':' with 0
      *separator = '\0';
      
      int sliderValue = atoi(command);
      ++separator;
      int muteValue = (*separator == 'M');

      setMute(sliderID,muteValue);
      
      sprintf(buffer,"Slider %d: value %d, mute: %d",sliderID,sliderValue,muteValue);
      Serial.println(buffer);

      
      
    }
    // Find the next command in input string
    command = strtok(0, "|");
    ++sliderID;
  }
}

void setMute(int s, int m) {
  analogSliderMute[s] = m;
 
}

void updateUI() {
   digitalWrite(ledPin, analogSliderMute[selectedSlider]);
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
