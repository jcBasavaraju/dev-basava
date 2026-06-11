// #pragma once

// #include <Arduino.h>

// enum ServoName : uint8_t {
//   R1 = 0, 
//   R2 = 1,
//   L1 = 2,
//   L2 = 3,
//   R4 = 4,
//   R3 = 5,
//   L3 = 6,
//   L4 = 7
// };

// const String ServoNames[]={"R1","R2","L1","L2","R4","R3","L3","L4"};

// inline int servoNameToIndex(const String& servo) {
//   if (servo == "L1") return L1;
//   if (servo == "L2") return L2;
//   if (servo == "L3") return L3;
//   if (servo == "L4") return L4;
//   if (servo == "R1") return R1;
//   if (servo == "R2") return R2;
//   if (servo == "R3") return R3;
//   if (servo == "R4") return R4;
//   return -1;
// }

// enum FaceAnimMode : uint8_t {
//   FACE_ANIM_LOOP = 0,
//   FACE_ANIM_ONCE = 1,
//   FACE_ANIM_BOOMERANG = 2
// };

// // External globals and helpers used by movement/pose sequences
// extern int frameDelay;
// extern int walkCycles;
// extern String currentCommand;

// extern void setServoAngle(uint8_t channel, int angle);
// extern void setFace(const String& faceName);
// extern void setFaceMode(FaceAnimMode mode);
// extern void setFaceWithMode(const String& faceName, FaceAnimMode mode);
// extern void delayWithFace(unsigned long ms);
// extern void enterIdle();
// extern bool pressingCheck(String cmd, int ms);

// // Pose/animation prototypes
// void runRestPose();
// void runStandPose(int face = 1);
// void runWavePose();
// void runDancePose();
// void runSwimPose();
// void runPointPose();
// void runPushupPose();
// void runBowPose();
// void runCutePose();
// void runFreakyPose();
// void runWormPose();
// void runShakePose();
// void runShrugPose();
// void runDeadPose();
// void runCrabPose();
// void runWalkPose();
// void runWalkBackward();
// void runTurnLeft();
// void runTurnRight();

// // ====== POSES ======
// inline void runRestPose() { 
//   Serial.println(F("REST")); 
//   setFaceWithMode("rest", FACE_ANIM_BOOMERANG); 
//   for (int i = 0; i < 8; i++) setServoAngle(i, 90); 
// }

// inline void runStandPose(int face) { 
//   Serial.println(F("STAND")); 
//   if (face == 1) setFaceWithMode("stand", FACE_ANIM_ONCE); 
//   setServoAngle(R1, 140); 
//   setServoAngle(R2, 115); 
//   setServoAngle(L1, 10); 
//   setServoAngle(L2, 22); 
//   setServoAngle(R4, 150); 
//   setServoAngle(R3, 117); 
//   setServoAngle(L3, 31); 
//   setServoAngle(L4, 10); 
//   if (face == 1) enterIdle();
// }

// inline void runWavePose() { 
//   Serial.println(F("WAVE")); 
//   setFaceWithMode("wave", FACE_ANIM_ONCE); 
//   runStandPose(0); 
//   delayWithFace(200);
//   setServoAngle(R4, 150); setServoAngle(L3, 31); 
//   setServoAngle(L3, 23); 
//   setServoAngle(L1, 120); 
//   delayWithFace(200);
//   setServoAngle(L2, 180); 
//   delayWithFace(300); 
//   for (int i = 0; i < 4; i++) { 
//     setServoAngle(L2, 180); delayWithFace(300); 
//     setServoAngle(L2, 0); delayWithFace(300); 
//   } 
//   runStandPose(1); 
//   if (currentCommand == "wave") currentCommand = "";
// }

// inline void runDancePose() { 
//   Serial.println(F("DANCE")); 
//   setFaceWithMode("dance", FACE_ANIM_LOOP); 
//   setServoAngle(R1, 140); setServoAngle(R2, 112); 
//   setServoAngle(L1, 10); setServoAngle(L2, 34); 
//   setServoAngle(R4, 88); setServoAngle(R3, 144); 
//   setServoAngle(L3, 36); setServoAngle(L4, 83); 
//   delayWithFace(300); 
//   for (int i = 0; i < 5; i++) { 
//     setServoAngle(R4, 115); setServoAngle(R3, 115); 
//     setServoAngle(L3, 10); setServoAngle(L4, 10); 
//     delayWithFace(300); 
//     setServoAngle(R4, 160); setServoAngle(R3, 160); 
//     setServoAngle(L3, 65); setServoAngle(L4, 65); 
//     delayWithFace(300); 
//   } 
//   runStandPose(1); 
//   if (currentCommand == "dance") currentCommand = "";
// }

// inline void runSwimPose() { 
//   Serial.println(F("SWIM")); 
//   setFaceWithMode("swim", FACE_ANIM_ONCE); 
//   for (int i = 0; i < 8; i++) setServoAngle(i, 90); 
//   for (int i = 0; i < 4; i++) { 
//     setServoAngle(R1, 71); setServoAngle(R2, 137); 
//     setServoAngle(L1, 73); setServoAngle(L2, 25); 
//     delayWithFace(400); 
//     setServoAngle(R1, 90); setServoAngle(R2, 32); 
//     setServoAngle(L1, 73); setServoAngle(L2, 138); 
//     delayWithFace(400); 
//   } 
//   runStandPose(1); 
//   if (currentCommand == "swim") currentCommand = "";
// }

// inline void runPointPose() { 
//   Serial.println(F("POINT")); 
//   setFaceWithMode("point", FACE_ANIM_BOOMERANG); 
//   setServoAngle(L2, 22); setServoAngle(R1, 145); 
//   setServoAngle(R2, 115); setServoAngle(L4, 10); 
//   setServoAngle(L1, 123); setServoAngle(L3, 31);
//   setServoAngle(R4, 150); setServoAngle(R3, 117); 
//   delayWithFace(2000); 
//   runStandPose(1); 
//   if (currentCommand == "point") currentCommand = "";
// }

// inline void runPushupPose() {
//   Serial.println(F("PUSHUP"));
//   setFaceWithMode("pushup", FACE_ANIM_ONCE);
//   runStandPose(0); 
//   delayWithFace(200);
//   setServoAngle(L1, 0);
//   setServoAngle(R1, 160);
//   setServoAngle(L3, 0);
//   setServoAngle(R3, 180);
//    setServoAngle(L2, 75);
//     setServoAngle(R2, 75);
//   delayWithFace(500);
//   for (int i = 0; i < 4; i++) {
//     setServoAngle(L4, 90);
//     setServoAngle(R4, 90);
//     delayWithFace(600);
//     setServoAngle(R1, 90);
//     setServoAngle(L1, 75);
//     delayWithFace(500);
//     setServoAngle(R1, 160);
//     setServoAngle(L1, 0);
//     delayWithFace(500);
//   }
//   runStandPose(1);
//   if (currentCommand == "pushup") currentCommand = "";
// }

// inline void runBowPose() {
//   Serial.println(F("BOW"));
//   setFaceWithMode("bow", FACE_ANIM_ONCE);
//   runStandPose(0); 
//   delayWithFace(200);
//   setServoAngle(L1, 180);
//   setServoAngle(R1, 0);
//   setServoAngle(L3, 40);
//   setServoAngle(R3, 110);
//   setServoAngle(L2, 0);
//   setServoAngle(R2, 145);
//   setServoAngle(R4, 180);
//   setServoAngle(L4, 0);
//   delayWithFace(600);
//   setServoAngle(L3, 0);
//   setServoAngle(R3, 180);
//   delayWithFace(3000);
//   runStandPose(1);
//   if (currentCommand == "bow") currentCommand = "";
// }

// inline void runCutePose() {
//   Serial.println(F("CUTE"));
//   setFaceWithMode("cute", FACE_ANIM_ONCE);
//   runStandPose(0); 
//   delayWithFace(200);
//   setServoAngle(L2, 160);
//   setServoAngle(R2, 20);
//   setServoAngle(R4, 180);
//   setServoAngle(L4, 0);

//   setServoAngle(L1, 0);
//   setServoAngle(R1, 180);
//   setServoAngle(L3, 180);
//   setServoAngle(R3, 0);
//   delayWithFace(200);
//   for (int i = 0; i < 5; i++) {
//     setServoAngle(R4, 180);
//     setServoAngle(L4, 45);
//     delayWithFace(300);
//     setServoAngle(R4, 135);
//     setServoAngle(L4, 0);
//     delayWithFace(300);
//   }
//   runStandPose(1);
//   if (currentCommand == "cute") currentCommand = "";
// }

// inline void runFreakyPose() {
//   Serial.println(F("FREAKY"));
//   setFaceWithMode("freaky", FACE_ANIM_ONCE);
//   runStandPose(0); 
//   delayWithFace(200);
//   setServoAngle(L1, 0);
//   setServoAngle(R1, 180);
//   setServoAngle(L2, 180);
//   setServoAngle(R2, 0);
//   setServoAngle(R4, 90);
//   setServoAngle(R3, 0);
//   delayWithFace(200);
//   for (int i = 0; i < 3; i++) {
//     setServoAngle(R3, 25);
//     delayWithFace(400);
//     setServoAngle(R3, 0);
//     delayWithFace(400);
//   }
//   runStandPose(1);
//   if (currentCommand == "freaky") currentCommand = "";
// }

// inline void runWormPose() {
//   Serial.println(F("WORM"));
//   setFaceWithMode("worm", FACE_ANIM_ONCE);
//   runStandPose(0);
//   delayWithFace(200);
//   setServoAngle(R1, 180); setServoAngle(R2, 0); setServoAngle(L1, 0); setServoAngle(L2, 180);
//   setServoAngle(R4, 90); setServoAngle(R3, 90); setServoAngle(L3, 90); setServoAngle(L4, 90);
//   delayWithFace(200);
//   for(int i=0; i<5; i++) {
//     setServoAngle(R3, 45); setServoAngle(L3, 135); setServoAngle(R4, 45); setServoAngle(L4, 135);
//     delayWithFace(300);
//     setServoAngle(R3, 135); setServoAngle(L3, 45); setServoAngle(R4, 135); setServoAngle(L4, 45);
//     delayWithFace(300);
//   }
//   runStandPose(1);
//   if (currentCommand == "worm") currentCommand = "";
// }

// inline void runShakePose() {
//   Serial.println(F("SHAKE"));
//   setFaceWithMode("shake", FACE_ANIM_ONCE);
//   runStandPose(0);
//   delayWithFace(200);
//   setServoAngle(R1, 135); setServoAngle(L1, 45); setServoAngle(L3, 90); setServoAngle(R3, 90);
//   setServoAngle(L2, 90); setServoAngle(R2, 90);
//   delayWithFace(200);
//   for(int i=0; i<5; i++) {
//     setServoAngle(R4, 45); setServoAngle(L4, 135);
//     delayWithFace(300);
//     setServoAngle(R4, 0); setServoAngle(L4, 180);
//     delayWithFace(300);
//   }
//   runStandPose(1);
//   if (currentCommand == "shake") currentCommand = "";
// }

// inline void runShrugPose() {
//   Serial.println(F("SHRUG"));
//   runStandPose(0);
//   setFaceWithMode("dead", FACE_ANIM_ONCE);
//   delayWithFace(200);
//   setServoAngle(R3, 90); setServoAngle(R4, 90); setServoAngle(L3, 90); setServoAngle(L4, 90);
//   delayWithFace(1000);
//   setFaceWithMode("shrug", FACE_ANIM_ONCE);
//   setServoAngle(R3, 0); setServoAngle(R4, 180); setServoAngle(L3, 180); setServoAngle(L4, 0);
//   delayWithFace(1500);
//   runStandPose(1);
//   if (currentCommand == "shrug") currentCommand = "";
// }

// inline void runDeadPose() {
//   Serial.println(F("DEAD"));
//   runStandPose(0);
//   setFaceWithMode("dead", FACE_ANIM_BOOMERANG);
//   delayWithFace(200);
//   setServoAngle(R3, 90); setServoAngle(R4, 90); setServoAngle(L3, 90); setServoAngle(L4, 90);
//   if (currentCommand == "dead") currentCommand = "";
// }

// inline void runCrabPose() {
//   Serial.println(F("CRAB"));
//   setFaceWithMode("crab", FACE_ANIM_ONCE);
//   runStandPose(0);
//   delayWithFace(200);
//   setServoAngle(R1, 90); setServoAngle(R2, 90); setServoAngle(L1, 90); setServoAngle(L2, 90);
//   setServoAngle(R4, 0); setServoAngle(R3, 180); setServoAngle(L3, 45); setServoAngle(L4, 135);
//   for(int i=0; i<5; i++) {
//     setServoAngle(R4, 45); setServoAngle(R3, 135); setServoAngle(L3, 0); setServoAngle(L4, 180);
//     delayWithFace(300);
//     setServoAngle(R4, 0); setServoAngle(R3, 180); setServoAngle(L3, 45); setServoAngle(L4, 135);
//     delayWithFace(300);
//   }
//   runStandPose(1);
//   if (currentCommand == "crab") currentCommand = "";
// }

// // --- MOVEMENT ANIMATIONS ---
// // inline void runWalkPose() {
// //   Serial.println(F("WALK FWD"));
// //   setFaceWithMode("walk", FACE_ANIM_ONCE);
// //   // Initial Stepstand dance 
// //   setServoAngle(R3, 135); setServoAngle(L3, 45);
// //   setServoAngle(R2, 100); setServoAngle(L1, 25);
// //   if (!pressingCheck("forward", frameDelay)) return;
  
// //   for (int i = 0; i < walkCycles; i++) {
// //     setServoAngle(R3, 135); setServoAngle(L3, 0);
// //     if (!pressingCheck("forward", frameDelay)) return;
// //     setServoAngle(L4, 0); setServoAngle(L2, 75);
// //     setServoAngle(R4, 180); setServoAngle(R1, 160);
// //     if (!pressingCheck("forward", frameDelay)) return;    
// //     setServoAngle(R2, 50); setServoAngle(L1, 00);
// //     if (!pressingCheck("forward", frameDelay)) return;
// //     setServoAngle(R4, 150); setServoAngle(L4, 0);
// //     if (!pressingCheck("forward", frameDelay)) return;
// //     setServoAngle(R3, 120); setServoAngle(L3, 20);
// //     setServoAngle(R2, 110); setServoAngle(L1, 0);
// //     if (!pressingCheck("forward", frameDelay)) return;  
// //     setServoAngle(L2, 0); setServoAngle(R1, 150);
// //     if (!pressingCheck("forward", frameDelay)) return;
// //   }
// //   runStandPose(1);
// // }
// inline void runWalkPose() {
//   Serial.println(F("WALK FWD"));
//   setFaceWithMode("walk", FACE_ANIM_ONCE);
//   // Initial
//   setServoAngle(R1, 150); setServoAngle(L1, 25);
//   setServoAngle(R2, 100); setServoAngle(L2, 75);
//   setServoAngle(R3, 135); setServoAngle(L3, 45);
//   setServoAngle(R4, 150); setServoAngle(L4, 10);
//   if (!pressingCheck("forward", frameDelay)) return;

//   for (int i = 0; i < walkCycles; i++) {
//     // Phase 1: R4 lifts forward → L4 mirrors it
//     setServoAngle(R1, 160); setServoAngle(L1, 25);
//     setServoAngle(R4, 180); setServoAngle(L4, 0);   // L4 mirrors R4
//     setServoAngle(R3, 135); setServoAngle(L3, 45);  // L3 mirrors R3
//     if (!pressingCheck("forward", frameDelay)) return;
//     setServoAngle(R2, 50);  setServoAngle(L2, 130); // arms swing
//     if (!pressingCheck("forward", frameDelay)) return;

//     // Phase 2: R3 steps → L3 mirrors it
//     setServoAngle(R3, 120); setServoAngle(L3, 60);  // L3 mirrors R3 (180-120=60)
//     setServoAngle(R4, 150); setServoAngle(L4, 10);  // L4 mirrors R4 back
//     setServoAngle(L1, 0);   setServoAngle(R1, 150);
//     if (!pressingCheck("forward", frameDelay)) return;
//     setServoAngle(R2, 110); setServoAngle(L2, 70);  // arms return
//     setServoAngle(R3, 135); setServoAngle(L3, 45);  // L3 mirrors R3 back
//     if (!pressingCheck("forward", frameDelay)) return;
//   }
//   runStandPose(1);
// }
// // // Logic reversed from Walk
// // inline void runWalkBackward() {
// //   Serial.println(F("WALK BACK"));
// //   setFaceWithMode("walk", FACE_ANIM_ONCE);
// //   if (!pressingCheck("backward", frameDelay)) return;
  
// //   for (int i = 0; i < walkCycles; i++) {
// //     setServoAngle(R3, 135); setServoAngle(L3, 0);
// //     if (!pressingCheck("backward", frameDelay)) return;
// //     setServoAngle(L4, 135); setServoAngle(L2, 135);
// //     setServoAngle(R4, 0); setServoAngle(R1, 90);
// //     if (!pressingCheck("backward", frameDelay)) return;    
// //     setServoAngle(R2, 90); setServoAngle(L1, 0);
// //     if (!pressingCheck("backward", frameDelay)) return;
// //     setServoAngle(R4, 45); setServoAngle(L4, 180);
// //     if (!pressingCheck("backward", frameDelay)) return;
// //     setServoAngle(R3, 180); setServoAngle(L3, 45);
// //     setServoAngle(R2, 45); setServoAngle(L1, 90);
// //     if (!pressingCheck("backward", frameDelay)) return;  
// //     setServoAngle(L2, 90); setServoAngle(R1, 180);
// //     if (!pressingCheck("backward", frameDelay)) return;
// //   }
// //   runStandPose(1);
// // }
// inline void runWalkBackward() {
//   Serial.println(F("WALK BACK"));
//   setFaceWithMode("walk", FACE_ANIM_ONCE);
//   // Mirror of forward initial position
//   setServoAngle(R1, 150); setServoAngle(L1, 25);
//   setServoAngle(R2, 100); setServoAngle(L2, 75);
//   setServoAngle(R3, 135); setServoAngle(L3, 38);
//   setServoAngle(R4, 150); setServoAngle(L4, 0);
//   if (!pressingCheck("backward", frameDelay)) return;

//   for (int i = 0; i < walkCycles; i++) {
//     // Phase 1: diagonal pair A (R1+L4) — push BACK (opposite of forward)
//     setServoAngle(R1, 150); setServoAngle(L4, 00);  // was 160/0 forward → now pull back
//     setServoAngle(R4, 180); setServoAngle(L1, 00);  // was 180/0 forward → now pull back
//     if (!pressingCheck("backward", frameDelay)) return;
//     setServoAngle(R2, 110); setServoAngle(L2, 50);  // arm swing reversed
//     if (!pressingCheck("backward", frameDelay)) return;
//     // Phase 2: diagonal pair B (L1+R4) — push BACK
//     setServoAngle(L1, 25);  setServoAngle(R4, 150); // reset to stand
//     setServoAngle(R1, 150); setServoAngle(L4, 5);
//     if (!pressingCheck("backward", frameDelay)) return;
//     setServoAngle(R2, 100); setServoAngle(L2, 75);  // arm swing reset
//     if (!pressingCheck("backward", frameDelay)) return;
//   }
//   runStandPose(1);
// }
// // Simple turn logic
// inline void runTurnLeft() {
//   Serial.println(F("TURN LEFT"));
//   setFaceWithMode("walk", FACE_ANIM_ONCE);
//   for (int i = 0; i < walkCycles; i++) {
//     //legset 1 (R1 L2)
//     setServoAngle(R3, 135); setServoAngle(L4, 135); 
//     if (!pressingCheck("left", frameDelay)) return;
//     setServoAngle(R1, 180); setServoAngle(L2, 180); 
//     if (!pressingCheck("left", frameDelay)) return;
//     setServoAngle(R3, 180); setServoAngle(L4, 180); 
//     if (!pressingCheck("left", frameDelay)) return;
//     setServoAngle(R1, 135); setServoAngle(L2, 135);
//     if (!pressingCheck("left", frameDelay)) return;
//       //legset 2 (R2 L1)
//     setServoAngle(R4, 45); setServoAngle(L3, 45); 
//     if (!pressingCheck("left", frameDelay)) return;
//     setServoAngle(R2, 90); setServoAngle(L1, 90); 
//     if (!pressingCheck("left", frameDelay)) return;
//     setServoAngle(R4, 0); setServoAngle(L3, 0); 
//     if (!pressingCheck("left", frameDelay)) return;
//     setServoAngle(R2, 45); setServoAngle(L1, 45);
//     if (!pressingCheck("left", frameDelay)) return;  
//   }
//   runStandPose(1);
// }

// inline void runTurnRight() {
//   Serial.println(F("TURN RIGHT"));
//   setFaceWithMode("walk", FACE_ANIM_ONCE);
//   for (int i = 0; i < walkCycles; i++) {
//     //legset 2 (R2 L1)
//     setServoAngle(R4, 45); setServoAngle(L3, 45); 
//     if (!pressingCheck("right", frameDelay)) return;
//     setServoAngle(R2, 0); setServoAngle(L1, 0); 
//     if (!pressingCheck("right", frameDelay)) return;
//     setServoAngle(R4, 0); setServoAngle(L3, 0); 
//     if (!pressingCheck("right", frameDelay)) return;
//     setServoAngle(R2, 45); setServoAngle(L1, 45);
//     if (!pressingCheck("right", frameDelay)) return;  
//     //legset 1 (R1 L2)
//     setServoAngle(R3, 135); setServoAngle(L4, 135); 
//     if (!pressingCheck("right", frameDelay)) return;
//     setServoAngle(R1, 90); setServoAngle(L2, 90); 
//     if (!pressingCheck("right", frameDelay)) return;
//     setServoAngle(R3, 180); setServoAngle(L4, 180); 
//     if (!pressingCheck("right", frameDelay)) return;
//     setServoAngle(R1, 135); setServoAngle(L2, 135);
//     if (!pressingCheck("right", frameDelay)) return;
//   }
//   runStandPose(1);
// }
#pragma once

#include <Arduino.h>

enum ServoName : uint8_t {
  R1 = 0, 
  R2 = 1,
  L1 = 2,
  L2 = 3,
  R4 = 4,
  R3 = 5,
  L3 = 6,
  L4 = 7
};

const String ServoNames[]={"R1","R2","L1","L2","R4","R3","L3","L4"};

inline int servoNameToIndex(const String& servo) {
  if (servo == "L1") return L1;
  if (servo == "L2") return L2;
  if (servo == "L3") return L3;
  if (servo == "L4") return L4;
  if (servo == "R1") return R1;
  if (servo == "R2") return R2;
  if (servo == "R3") return R3;
  if (servo == "R4") return R4;
  return -1;
}

enum FaceAnimMode : uint8_t {
  FACE_ANIM_LOOP = 0,
  FACE_ANIM_ONCE = 1,
  FACE_ANIM_BOOMERANG = 2
};

extern int frameDelay;
extern int walkCycles;
extern String currentCommand;

extern void setServoAngle(uint8_t channel, int angle);
extern void setFace(const String& faceName);
extern void setFaceMode(FaceAnimMode mode);
extern void setFaceWithMode(const String& faceName, FaceAnimMode mode);
extern void delayWithFace(unsigned long ms);
extern void enterIdle();
extern bool pressingCheck(String cmd, int ms);

void runRestPose();
void runStandPose(int face = 1);
void runWavePose();
void runDancePose();
void runSwimPose();
void runPointPose();
void runPushupPose();
void runBowPose();
void runCutePose();
void runFreakyPose();
void runWormPose();
void runShakePose();
void runShrugPose();
void runDeadPose();
void runCrabPose();
void runWalkPose();
void runWalkBackward();
void runTurnLeft();
void runTurnRight();

// ====== POSES ======
inline void runRestPose() { 
  Serial.println(F("REST")); 
  setFaceWithMode("rest", FACE_ANIM_BOOMERANG); 
  for (int i = 0; i < 8; i++) setServoAngle(i, 90); 
}

inline void runStandPose(int face) { 
  Serial.println(F("STAND")); 
  if (face == 1) setFaceWithMode("stand", FACE_ANIM_ONCE); 
  setServoAngle(R1, 140); 
  setServoAngle(R2, 115); 
  setServoAngle(L1, 10); 
  setServoAngle(L2, 22); 
  setServoAngle(R4, 150); 
  setServoAngle(R3, 117); 
  setServoAngle(L3, 31); 
  setServoAngle(L4, 10); 
  if (face == 1) enterIdle();
}

inline void runWavePose() { 
  Serial.println(F("WAVE")); 
  setFaceWithMode("wave", FACE_ANIM_ONCE); 
  runStandPose(0); 
  delayWithFace(200);
  // Legs stay stable
  setServoAngle(R1, 140); setServoAngle(L1, 10);
  setServoAngle(R4, 150); setServoAngle(L4, 10);
  // Raise L3 arm up
  setServoAngle(L3, 120);
  delayWithFace(200);
  // Wave L2 back and forth
  for (int i = 0; i < 4; i++) { 
    setServoAngle(L2, 170); delayWithFace(300); 
    setServoAngle(L2, 10);  delayWithFace(300); 
  } 
  runStandPose(1); 
  if (currentCommand == "wave") currentCommand = "";
}

inline void runDancePose() { 
  Serial.println(F("DANCE")); 
  setFaceWithMode("dance", FACE_ANIM_LOOP); 
  setServoAngle(R1, 140); setServoAngle(R2, 112); 
  setServoAngle(L1, 10);  setServoAngle(L2, 34); 
  setServoAngle(R4, 88);  setServoAngle(R3, 144); 
  setServoAngle(L3, 36);  setServoAngle(L4, 83); 
  delayWithFace(300); 
  for (int i = 0; i < 5; i++) { 
    setServoAngle(R4, 115); setServoAngle(R3, 115); 
    setServoAngle(L3, 10);  setServoAngle(L4, 10); 
    delayWithFace(300); 
    setServoAngle(R4, 160); setServoAngle(R3, 160); 
    setServoAngle(L3, 65);  setServoAngle(L4, 65); 
    delayWithFace(300); 
  } 
  runStandPose(1); 
  if (currentCommand == "dance") currentCommand = "";
}

inline void runSwimPose() { 
  Serial.println(F("SWIM")); 
  setFaceWithMode("swim", FACE_ANIM_ONCE); 
  for (int i = 0; i < 8; i++) setServoAngle(i, 90); 
  for (int i = 0; i < 4; i++) { 
    setServoAngle(R1, 71); setServoAngle(R2, 137); 
    setServoAngle(L1, 73); setServoAngle(L2, 25); 
    delayWithFace(400); 
    setServoAngle(R1, 90); setServoAngle(R2, 32); 
    setServoAngle(L1, 73); setServoAngle(L2, 138); 
    delayWithFace(400); 
  } 
  runStandPose(1); 
  if (currentCommand == "swim") currentCommand = "";
}

inline void runPointPose() { 
  Serial.println(F("POINT")); 
  setFaceWithMode("point", FACE_ANIM_BOOMERANG); 
  setServoAngle(L2, 22);  setServoAngle(R1, 145); 
  setServoAngle(R2, 115); setServoAngle(L4, 10); 
  setServoAngle(L1, 123); setServoAngle(L3, 31);
  setServoAngle(R4, 150); setServoAngle(R3, 117); 
  delayWithFace(2000); 
  runStandPose(1); 
  if (currentCommand == "point") currentCommand = "";
}

inline void runPushupPose() {
  Serial.println(F("PUSHUP"));
  setFaceWithMode("pushup", FACE_ANIM_ONCE);
  runStandPose(0); 
  delayWithFace(200);
  setServoAngle(L1, 0);
  setServoAngle(R1, 160);
  setServoAngle(L3, 0);
  setServoAngle(R3, 180);
  setServoAngle(L2, 75);
  setServoAngle(R2, 75);
  delayWithFace(500);
  for (int i = 0; i < 4; i++) {
    setServoAngle(L4, 90);
    setServoAngle(R4, 90);
    delayWithFace(600);
    setServoAngle(R1, 90);
    setServoAngle(L1, 75);
    delayWithFace(500);
    setServoAngle(R1, 160);
    setServoAngle(L1, 0);
    delayWithFace(500);
  }
  runStandPose(1);
  if (currentCommand == "pushup") currentCommand = "";
}

inline void runBowPose() {
  Serial.println(F("BOW"));
  setFaceWithMode("bow", FACE_ANIM_ONCE);
  runStandPose(0); 
  delayWithFace(200);
  setServoAngle(L1, 180);
  setServoAngle(R1, 0);
  setServoAngle(L3, 40);
  setServoAngle(R3, 110);
  setServoAngle(L2, 0);
  setServoAngle(R2, 145);
  setServoAngle(R4, 180);
  setServoAngle(L4, 0);
  delayWithFace(600);
  setServoAngle(L3, 0);
  setServoAngle(R3, 140);
  delayWithFace(3000);
  runStandPose(1);
  if (currentCommand == "bow") currentCommand = "";
}

inline void runCutePose() {
  Serial.println(F("CUTE"));
  setFaceWithMode("cute", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  // Arms tuck in
  setServoAngle(R2, 20);  setServoAngle(L2, 160);
  setServoAngle(R3, 0);   setServoAngle(L3, 180);
  delayWithFace(200);
  // Legs bounce symmetrically
  for (int i = 0; i < 5; i++) {
    setServoAngle(R1, 160); setServoAngle(L1, 20);
    setServoAngle(R4, 160); setServoAngle(L4, 20);
    delayWithFace(300);
    setServoAngle(R1, 130); setServoAngle(L1, 40);
    setServoAngle(R4, 130); setServoAngle(L4, 40);
    delayWithFace(300);
  }
  runStandPose(1);
  if (currentCommand == "cute") currentCommand = "";
}

inline void runFreakyPose() {
  Serial.println(F("FREAKY"));
  setFaceWithMode("freaky", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  // Arms spread wide
  setServoAngle(R2, 0);   setServoAngle(L2, 180);
  setServoAngle(R3, 0);   setServoAngle(L3, 180);
  // Legs stay stable
  setServoAngle(R1, 140); setServoAngle(L1, 10);
  setServoAngle(R4, 150); setServoAngle(L4, 10);
  delayWithFace(200);
  // Twitch arms symmetrically
  for (int i = 0; i < 3; i++) {
    setServoAngle(R2, 25);  setServoAngle(L2, 155);
    setServoAngle(R3, 25);  setServoAngle(L3, 155);
    delayWithFace(400);
    setServoAngle(R2, 0);   setServoAngle(L2, 180);
    setServoAngle(R3, 0);   setServoAngle(L3, 180);
    delayWithFace(400);
  }
  runStandPose(1);
  if (currentCommand == "freaky") currentCommand = "";
}

inline void runWormPose() {
  Serial.println(F("WORM"));
  setFaceWithMode("worm", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  setServoAngle(R1, 180); setServoAngle(R2, 0);
  setServoAngle(L1, 0);   setServoAngle(L2, 180);
  setServoAngle(R4, 90);  setServoAngle(R3, 90);
  setServoAngle(L3, 90);  setServoAngle(L4, 90);
  delayWithFace(200);
  for (int i = 0; i < 5; i++) {
    setServoAngle(R3, 45);  setServoAngle(L3, 135);
    setServoAngle(R4, 45);  setServoAngle(L4, 135);
    delayWithFace(300);
    setServoAngle(R3, 135); setServoAngle(L3, 45);
    setServoAngle(R4, 135); setServoAngle(L4, 45);
    delayWithFace(300);
  }
  runStandPose(1);
  if (currentCommand == "worm") currentCommand = "";
}

inline void runShakePose() {
  Serial.println(F("SHAKE"));
  setFaceWithMode("shake", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  // Arms held level
  setServoAngle(R2, 90);  setServoAngle(L2, 90);
  setServoAngle(R3, 90);  setServoAngle(L3, 90);
  delayWithFace(200);
  // Legs shake side to side
  for (int i = 0; i < 5; i++) {
    setServoAngle(R1, 160); setServoAngle(L1, 20);
    setServoAngle(R4, 160); setServoAngle(L4, 20);
    delayWithFace(300);
    setServoAngle(R1, 120); setServoAngle(L1, 50);
    setServoAngle(R4, 120); setServoAngle(L4, 50);
    delayWithFace(300);
  }
  runStandPose(1);
  if (currentCommand == "shake") currentCommand = "";
}

inline void runShrugPose() {
  Serial.println(F("SHRUG"));
  runStandPose(0);
  setFaceWithMode("dead", FACE_ANIM_ONCE);
  delayWithFace(200);
  setServoAngle(R3, 90); setServoAngle(R4, 90);
  setServoAngle(L3, 90); setServoAngle(L4, 90);
  delayWithFace(1000);
  setFaceWithMode("shrug", FACE_ANIM_ONCE);
  setServoAngle(R3, 0);  setServoAngle(R4, 180);
  setServoAngle(L3, 180);setServoAngle(L4, 0);
  delayWithFace(1500);
  runStandPose(1);
  if (currentCommand == "shrug") currentCommand = "";
}

inline void runDeadPose() {
  Serial.println(F("DEAD"));
  runStandPose(0);
  setFaceWithMode("dead", FACE_ANIM_BOOMERANG);
  delayWithFace(200);
  setServoAngle(R3, 90); setServoAngle(R4, 90);
  setServoAngle(L3, 90); setServoAngle(L4, 90);
  if (currentCommand == "dead") currentCommand = "";
}

inline void runCrabPose() {
  Serial.println(F("CRAB"));
  setFaceWithMode("crab", FACE_ANIM_ONCE);
  runStandPose(0);
  delayWithFace(200);
  // Arms out wide
  setServoAngle(R2, 90); setServoAngle(L2, 90);
  setServoAngle(R3, 90); setServoAngle(L3, 90);
  delayWithFace(200);
  // Crab step: legs alternate in mirror
  for (int i = 0; i < 5; i++) {
    setServoAngle(R1, 170); setServoAngle(L4, 0);
    delayWithFace(300);
    setServoAngle(R1, 140); setServoAngle(L4, 10);
    setServoAngle(L1, 0);   setServoAngle(R4, 170);
    delayWithFace(300);
    setServoAngle(L1, 10);  setServoAngle(R4, 150);
    delayWithFace(200);
  }
  runStandPose(1);
  if (currentCommand == "crab") currentCommand = "";
}
// inline void runWalkPose() {
//   Serial.println(F("WALK FWD"));
//   setFaceWithMode("walk", FACE_ANIM_ONCE);
//   // Initial position
//   setServoAngle(R1, 150); setServoAngle(L1, 15);
//   setServoAngle(R2, 100); setServoAngle(L2, 75);
//   setServoAngle(R3, 135); setServoAngle(L3, 49);  // set once, never change
//   setServoAngle(R4, 150); setServoAngle(L4, 0);
//   if (!pressingCheck("forward", frameDelay)) return;

//   for (int i = 0; i < walkCycles; i++) {
//     // Phase 1: only R4/L4 push
//     setServoAngle(R1, 160); setServoAngle(L1, 15);
//     setServoAngle(R4, 180); setServoAngle(L4, 30);
//     if (!pressingCheck("forward", frameDelay)) return;
//     setServoAngle(R2, 50);  setServoAngle(L2, 130);
//     if (!pressingCheck("forward", frameDelay)) return;
//     // Phase 2: only R4/L4 return
//     setServoAngle(R4, 150); setServoAngle(L4, 0);
//     setServoAngle(L1, 0);   setServoAngle(R1, 150);
//     if (!pressingCheck("forward", frameDelay)) return;
//     setServoAngle(R2, 110); setServoAngle(L2, 70);
//     if (!pressingCheck("forward", frameDelay)) return;
//   }
//   runStandPose(1);
// }
// inline void runWalkBackward() {
//   Serial.println(F("WALK BACK"));
//   setFaceWithMode("walk", FACE_ANIM_ONCE);
//   setServoAngle(R1, 150); setServoAngle(L1, 15);
//   setServoAngle(R2, 130); setServoAngle(L2, 30);  // constant
//   setServoAngle(R3, 150); setServoAngle(L3, 20);
//   setServoAngle(R4, 150); setServoAngle(L4, 0);
//   if (!pressingCheck("backward", frameDelay)) return;

//   for (int i = 0; i < walkCycles; i++) {
//     // Phase 1
//     setServoAngle(R1, 180); setServoAngle(L1, 0);
//     setServoAngle(R4, 180); setServoAngle(L4, 30);
//     if (!pressingCheck("backward", frameDelay)) return;
//     setServoAngle(R3, 90);  setServoAngle(L3, 75);  // second step like R2/L2 in forward
//     if (!pressingCheck("backward", frameDelay)) return;
//     // Phase 2
//     setServoAngle(R4, 150); setServoAngle(L4, 0);
//     setServoAngle(L1, 40);  setServoAngle(R1, 130);
//     if (!pressingCheck("backward", frameDelay)) return;
//     setServoAngle(R3, 150); setServoAngle(L3, 20);  // second step like R2/L2 in forward
//     if (!pressingCheck("backward", frameDelay)) return;
//   }
//   runStandPose(1);
// }
inline void runWalkPose() {
  Serial.println(F("WALK FWD"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);

  // Neutral ready stance
  setServoAngle(R1, 150); setServoAngle(L1, 15);
  setServoAngle(R2, 100); setServoAngle(L2, 75);
  setServoAngle(R3, 135); setServoAngle(L3, 49);
  setServoAngle(R4, 150); setServoAngle(L4, 15);
  if (!pressingCheck("forward", frameDelay)) return;

  for (int i = 0; i < walkCycles; i++) {

    // --- Phase 1: Lift diagonal pair A (R1+L4), shift weight to B (R4+L1) ---
    setServoAngle(R4, 165); setServoAngle(L4, 30);   // B legs push down/grip
    setServoAngle(L1, 5);   setServoAngle(R1, 165);  // A legs lift
    if (!pressingCheck("forward", frameDelay)) return;

    // --- Phase 2: Swing diagonal A forward ---
    setServoAngle(R2, 60);  setServoAngle(L2, 120);  // A body joints swing
    if (!pressingCheck("forward", frameDelay)) return;

    // --- Phase 3: Plant A, lift diagonal pair B (R4+L1), shift weight to A ---
    setServoAngle(R1, 145); setServoAngle(L1, 20);   // A legs plant
    setServoAngle(R4, 150); setServoAngle(L4, 5);    // B legs lift slightly
    if (!pressingCheck("forward", frameDelay)) return;

    // --- Phase 4: Swing diagonal B forward, return A joints ---
    setServoAngle(R2, 110); setServoAngle(L2, 68);   // B body joints swing / A return
    if (!pressingCheck("forward", frameDelay)) return;
  }

  runStandPose(1);
}

inline void runWalkBackward() {
  Serial.println(F("WALK BACK"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);

  // Neutral ready stance
  setServoAngle(R1, 150); setServoAngle(L1, 15);
  setServoAngle(R2, 120); setServoAngle(L2, 55);
  setServoAngle(R3, 150); setServoAngle(L3, 25);
  setServoAngle(R4, 150); setServoAngle(L4, 15);
  if (!pressingCheck("backward", frameDelay)) return;

  for (int i = 0; i < walkCycles; i++) {

    // --- Phase 1: Lift diagonal A (R1+L4), B legs grip ---
    setServoAngle(R4, 165); setServoAngle(L4, 30);   // B grip
    setServoAngle(L1, 5);   setServoAngle(R1, 165);  // A lift
    if (!pressingCheck("backward", frameDelay)) return;

    // --- Phase 2: Swing diagonal A backward ---
    setServoAngle(R3, 100); setServoAngle(L3, 65);   // A rear joints pull back
    if (!pressingCheck("backward", frameDelay)) return;

    // --- Phase 3: Plant A, lift diagonal B ---
    setServoAngle(R1, 145); setServoAngle(L1, 20);   // A plant
    setServoAngle(R4, 150); setServoAngle(L4, 5);    // B lift
    if (!pressingCheck("backward", frameDelay)) return;

    // --- Phase 4: Swing B backward, return A joints ---
    setServoAngle(R3, 150); setServoAngle(L3, 25);   // B rear return / A settle
    if (!pressingCheck("backward", frameDelay)) return;
  }

  runStandPose(1);
}

inline void runTurnLeft() {
  Serial.println(F("TURN LEFT"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);
  setServoAngle(R1, 150); setServoAngle(L1, 15);
  setServoAngle(R2, 100); setServoAngle(L2, 75);
  setServoAngle(R3, 135); setServoAngle(L3, 49);
  setServoAngle(R4, 150); setServoAngle(L4, 0);
  if (!pressingCheck("left", frameDelay)) return;

  for (int i = 0; i < walkCycles; i++) {
    // R side pushes forward, L side pushes backward
    setServoAngle(R1, 160); setServoAngle(L1, 40);
    setServoAngle(R4, 180); setServoAngle(L4, 30);
    if (!pressingCheck("left", frameDelay)) return;
    setServoAngle(R2, 50);  setServoAngle(L2, 50);
    if (!pressingCheck("left", frameDelay)) return;
    // return
    setServoAngle(R4, 150); setServoAngle(L4, 0);
    setServoAngle(R1, 150); setServoAngle(L1, 15);
    if (!pressingCheck("left", frameDelay)) return;
    setServoAngle(R2, 110); setServoAngle(L2, 75);
    if (!pressingCheck("left", frameDelay)) return;
  }
  runStandPose(1);
}

inline void runTurnRight() {
  Serial.println(F("TURN RIGHT"));
  setFaceWithMode("walk", FACE_ANIM_ONCE);
  setServoAngle(R1, 150); setServoAngle(L1, 15);
  setServoAngle(R2, 100); setServoAngle(L2, 75);
  setServoAngle(R3, 135); setServoAngle(L3, 49);
  setServoAngle(R4, 150); setServoAngle(L4, 0);
  if (!pressingCheck("right", frameDelay)) return;

  for (int i = 0; i < walkCycles; i++) {
    // L side pushes forward, R side pushes backward
    setServoAngle(L1, 0);   setServoAngle(R1, 120);
    setServoAngle(L4, 30);  setServoAngle(R4, 120);
    if (!pressingCheck("right", frameDelay)) return;
    setServoAngle(L2, 130); setServoAngle(R2, 130);
    if (!pressingCheck("right", frameDelay)) return;
    // return
    setServoAngle(L4, 0);   setServoAngle(R4, 150);
    setServoAngle(L1, 15);  setServoAngle(R1, 150);
    if (!pressingCheck("right", frameDelay)) return;
    setServoAngle(L2, 75);  setServoAngle(R2, 100);
    if (!pressingCheck("right", frameDelay)) return;
  }
  runStandPose(1);
}


