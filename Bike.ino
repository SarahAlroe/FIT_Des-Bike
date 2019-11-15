#include <movingAvg.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
#include <EasyButton.h>
#include<Wire.h>

const int MPU = 0x68;

const int lfPin = 2;
const int rfPin = 3;
const int lbPin = 4;
const int rbPin = 5;

const int lButtonPin = 6;
const int rButtonPin = 7;
const int vibPin = 8;

const int frontNumPixels = 16;
const int backNumPixels = 12;
const uint16_t AnimCount = 1;

const RgbwColor frontIdleColor = RgbwColor(0, 0, 0, 50);
const RgbColor backIdleColor = RgbColor(50, 0, 0);
const RgbColor backIdleColorBreaking = RgbColor(255, 0, 0);
const RgbColor turnColor = RgbColor(255, 50, 0);
const RgbColor invTurnColor = RgbColor(0);
const int turnDarkenAmount = 1;
const int animationSpeed = 600;

const float vibPart = 0.25;

bool breaking = false;
bool turningLeft = false;
bool turningRight = false;

NeoGamma<NeoGammaTableMethod> colorGamma;
NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> lfstrip(frontNumPixels, lfPin);
NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> rfstrip(frontNumPixels, rfPin);
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> lbstrip(backNumPixels, lbPin);
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> rbstrip(backNumPixels, rbPin);
NeoPixelAnimator animations(AnimCount);

EasyButton lButton(lButtonPin);
EasyButton rButton(rButtonPin);

const int accFwOff = -874; //Experimentally reached value (Note: very device orientationd dependent)
const int breakingThreshold = 900; //Same
const int accMovingAvgCount = 10;
const int accReadDelay = 100;
unsigned long accLastRead = 0;
movingAvg avgAccFw(accMovingAvgCount);

void lButtonPressedCallBack() {
  if (turningLeft && turningRight) {
    turningLeft = false;
    turningRight = false;
  } else {
    turningLeft = ! turningLeft;
  }
}

void rButtonPressedCallBack() {
  if (turningLeft && turningRight) {
    turningLeft = false;
    turningRight = false;
  } else {
    turningRight = ! turningRight;
  }
}

void handleAnimState() {
  if (turningLeft && turningRight) {
    animations.StartAnimation(0, animationSpeed, ForwardAnimUpdate);
    return;
  }
  if (turningLeft) {
    animations.StartAnimation(0, animationSpeed, LeftAnimUpdate);
    return;
  }
  if (turningRight) {
    animations.StartAnimation(0, animationSpeed, RightAnimUpdate);
    return;
  }
  animations.StartAnimation(0, animationSpeed, IdleAnimUpdate);
}

int towardsClamped(int start, int target, int steps) {
  if (start == target) {
    return start;
  }
  int diff = target - start;
  int sign = (diff > 0) - (diff < 0);
  int dist = sign * steps;
  if (abs(dist) > abs(diff)) {
    dist = diff;
  }
  return start + dist;
}

RgbColor colorTowards(RgbColor start, RgbColor target, int maxAmount) {
  if (start.R == target.R && start.G == target.G && start.B == target.B) {
    return start;
  }

  int r = towardsClamped(start.R, target.R, maxAmount);
  int g = towardsClamped(start.G, target.G, maxAmount);
  int b = towardsClamped(start.B, target.B, maxAmount);

  return RgbColor(r, g, b);
}
RgbwColor colorTowards(RgbwColor start, RgbwColor target, int maxAmount) {
  if (start.R == target.R && start.G == target.G && start.B == target.B && start.W == target.W) {
    return start;
  }

  int r = towardsClamped(start.R, target.R, maxAmount);
  int g = towardsClamped(start.G, target.G, maxAmount);
  int b = towardsClamped(start.B, target.B, maxAmount);
  int w = towardsClamped(start.W, target.W, maxAmount);

  return RgbwColor(r, g, b, w);
}

void fadeStripTowards(NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> &strip, RgbColor color, int amount) {
  for (uint16_t indexPixel = 0; indexPixel < strip.PixelCount(); indexPixel++)
  {
    strip.SetPixelColor(indexPixel,
                        colorTowards(strip.GetPixelColor(indexPixel), color, amount));
  }
}
void fadeStripTowards(NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> &strip, RgbwColor color, int amount) {
  for (uint16_t indexPixel = 0; indexPixel < strip.PixelCount(); indexPixel++)
  {
    strip.SetPixelColor(indexPixel,
                        colorTowards(strip.GetPixelColor(indexPixel), color, amount));
  }
}
void fadeIntervalTowards(NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> &strip, int i0, int i1, RgbColor color, int amount) {
  for (int i = i0; i <= i1; i++) {
    strip.SetPixelColor(i, colorTowards(strip.GetPixelColor(i), color, amount));
  }
}
void fadeIntervalTowards(NeoPixelBus<NeoGrbwFeature, Neo800KbpsMethod> &strip, int i0, int i1, RgbwColor color, int amount) {
  for (int i = i0; i <= i1; i++) {
    strip.SetPixelColor(i, colorTowards(strip.GetPixelColor(i), color, amount));
  }
}

void handleVib(float progress){
  if (progress<vibPart){
    digitalWrite(vibPin, HIGH);
  }else{
    digitalWrite(vibPin, LOW);
  }
}

void IdleAnimUpdate(const AnimationParam& param) {
  fadeStripTowards(lfstrip, frontIdleColor, turnDarkenAmount);
  fadeStripTowards(rfstrip, frontIdleColor, turnDarkenAmount);
  RgbColor backColor = breaking ? backIdleColorBreaking : backIdleColor;
  fadeStripTowards(lbstrip, backColor, turnDarkenAmount);
  fadeStripTowards(rbstrip, backColor, turnDarkenAmount);

  if (param.state == AnimationState_Completed)
  {
    handleAnimState();
  }
}

void ForwardAnimUpdate(const AnimationParam& param) {
  RgbColor backTurnColor = breaking ? backIdleColor : invTurnColor;
  float progress = param.progress;
  int index;

  handleVib(progress);

  fadeStripTowards(lfstrip, invTurnColor, turnDarkenAmount);
  index = (1.0f - progress) * lfstrip.PixelCount();
  fadeIntervalTowards(lfstrip, index - 1, index + 1, turnColor, turnDarkenAmount * 2);

  fadeStripTowards(lbstrip, backTurnColor, turnDarkenAmount);
  index = (1.0f - progress) * lbstrip.PixelCount();
  fadeIntervalTowards(lbstrip, index - 1, index + 1, turnColor, turnDarkenAmount * 2);

  fadeStripTowards(rfstrip, invTurnColor, turnDarkenAmount);
  index = (1.0f - progress) * rfstrip.PixelCount();
  fadeIntervalTowards(rfstrip, index - 1, index + 1, turnColor, turnDarkenAmount * 2);

  fadeStripTowards(rbstrip, backTurnColor, turnDarkenAmount);
  index = (1.0f - progress) * rbstrip.PixelCount();
  fadeIntervalTowards(rbstrip, index - 1, index + 1, turnColor, turnDarkenAmount * 2);


  if (param.state == AnimationState_Completed)
  {
    handleAnimState();
  }
}

void LeftAnimUpdate(const AnimationParam& param) {
  RgbColor backColor = breaking ? backIdleColorBreaking : backIdleColor;
  RgbColor backTurnColor = breaking ? backIdleColor : invTurnColor;

  float progress = param.progress;
  int index;

  handleVib(progress);

  fadeStripTowards(lfstrip, invTurnColor, turnDarkenAmount);
  index = progress * lfstrip.PixelCount();
  fadeIntervalTowards(lfstrip, index - 1, index + 1, turnColor, turnDarkenAmount * 2);

  fadeStripTowards(lbstrip, backTurnColor, turnDarkenAmount);
  index = progress * lbstrip.PixelCount();
  fadeIntervalTowards(lbstrip, index - 1, index + 1, turnColor, turnDarkenAmount * 2);

  fadeStripTowards(rfstrip, frontIdleColor, turnDarkenAmount);
  fadeStripTowards(rbstrip, backColor, turnDarkenAmount);

  if (param.state == AnimationState_Completed)
  {
    handleAnimState();
  }
}

void RightAnimUpdate(const AnimationParam& param) {
  RgbColor backColor = breaking ? backIdleColorBreaking : backIdleColor;
  RgbColor backTurnColor = breaking ? backIdleColor : invTurnColor;

  float progress = param.progress;
  int index;

  handleVib(progress);

  fadeStripTowards(rfstrip, invTurnColor, turnDarkenAmount);
  index = progress * rfstrip.PixelCount();
  fadeIntervalTowards(rfstrip, index - 1, index + 1, turnColor, turnDarkenAmount * 2);

  fadeStripTowards(rbstrip, backTurnColor, turnDarkenAmount);
  index = progress * rbstrip.PixelCount();
  fadeIntervalTowards(rbstrip, index - 1, index + 1, turnColor, turnDarkenAmount * 2);

  fadeStripTowards(lfstrip, frontIdleColor, turnDarkenAmount);
  fadeStripTowards(lbstrip, backColor, turnDarkenAmount);

  if (param.state == AnimationState_Completed)
  {
    handleAnimState();
  }
}

void setupAccelerometer() {
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

int16_t readAcceleration() {
  if (accLastRead + accReadDelay < millis()) {
    accLastRead = millis();
    Wire.beginTransmission(MPU);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU, 2, true);
    return avgAccFw.reading((Wire.read() << 8 | Wire.read()) + accFwOff);
  }else{
    return avgAccFw.getAvg();
  }

}

void setup() {
  pinMode(vibPin, OUTPUT);
  setupAccelerometer();

  lButton.begin();
  rButton.begin();
  lButton.onPressed(lButtonPressedCallBack);
  rButton.onPressed(rButtonPressedCallBack);

  avgAccFw.begin();

  lfstrip.Begin();
  rfstrip.Begin();
  lbstrip.Begin();
  rbstrip.Begin();

  lfstrip.Show();
  rfstrip.Show();
  lbstrip.Show();
  rbstrip.Show();

  animations.StartAnimation(0, animationSpeed, IdleAnimUpdate);
}

void loop() {
  lButton.read();
  rButton.read();

  breaking = readAcceleration() > breakingThreshold;

  animations.UpdateAnimations();
  lfstrip.Show();
  rfstrip.Show();
  lbstrip.Show();
  rbstrip.Show();
}
