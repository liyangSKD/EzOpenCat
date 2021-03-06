#include "servo_animator.h"

#include "eeprom_settings.h"

#include <gtest/gtest.h>

class ServoAnimatorTest : public testing::Test {
 protected:
  ServoAnimatorTest() {}

  void SetUp() override {
    animator_.Initialize();
    animator_.SetEepromSettings(&settings_);
    const int8_t* rest_frame = animator_.GetFrame(kAnimationRest, 0);

    for (int i = 0; i < kServoCount; ++i) {
      rest_positions_[i] = 90 + rest_frame[i] * animator_.kDirectionMap[i];
      settings_.servo_zero_offset[i] = 0;
      settings_.servo_upper_extents[i] = 90;
      settings_.servo_lower_extents[i] = -90;
    }
  }

  void TestAnimate(int servo, int* test_ms, int* expected_angle, int count);

  ServoAnimator animator_;
  int rest_positions_[kServoCount];
  EepromSettings settings_;
};

TEST_F(ServoAnimatorTest, Initialize) {
  EXPECT_FALSE(animator_.animating());
}

TEST_F(ServoAnimatorTest, GetFrameForCalibrationPose) {
  const int8_t* frame =
    animator_.GetFrame(kAnimationCalibrationPose, 0);
  for (int i = 0; i < kServoCount; ++i)
    EXPECT_EQ(0, frame[i]);
}

TEST_F(ServoAnimatorTest, GetFrameForActualFrameAnimation) {
  const int8_t* frame =
    animator_.GetFrame(kAnimationFistBump, 0);
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(-50, frame[kServoHead]);
  EXPECT_EQ(-80, frame[kServoRightBackShoulder]);
  frame = animator_.GetFrame(kAnimationFistBump, 1);
  ASSERT_NE(nullptr, frame);
  EXPECT_EQ(-20, frame[kServoHead]);
  EXPECT_EQ(-80, frame[kServoRightBackShoulder]);
  frame = animator_.GetFrame(kAnimationFistBump, 2);
  EXPECT_EQ(nullptr, frame);
}

TEST_F(ServoAnimatorTest, AttachAttachesAndSetsToRestingPosition) {
  for (int i = 0; i < kServoCount; ++i)
    EXPECT_FALSE(animator_.servo_[i]->attached);
  animator_.Attach();

  for (int i = 0; i < kServoCount; ++i) {
    EXPECT_TRUE(animator_.servo_[i]->attached);
    EXPECT_EQ(rest_positions_[i], animator_.servo_[i]->value);
  }
}

TEST_F(ServoAnimatorTest, DetachDetaches) {
  animator_.Attach();
  animator_.Detach();
  for (int i = 0; i < kServoCount; ++i)
    EXPECT_FALSE(animator_.servo_[i]->attached);
}

TEST_F(ServoAnimatorTest, StartFrameToCalibrationAndAnimateConverges) {
  const int8_t* frame =
    animator_.GetFrame(kAnimationCalibrationPose, 0);
  animator_.Attach();
  // We tested above that Attach is now at resting position.

  animator_.StartFrame(frame, 0);

  for (int i = 0; i < kServoCount; ++i) {
    EXPECT_TRUE(animator_.servo_[i]->attached);
    EXPECT_EQ(rest_positions_[i], animator_.servo_[i]->value);
  }

  EXPECT_TRUE(animator_.animating());

  animator_.Animate(10000);  // 10 second later...

  EXPECT_FALSE(animator_.animating());

  for (int i = 0; i < kServoCount; ++i) {
    EXPECT_TRUE(animator_.servo_[i]->attached);
    EXPECT_EQ(90, animator_.servo_[i]->value);
  }
}

TEST_F(ServoAnimatorTest, StartFrameToCalibrationWithZeroOffsets) {
  settings_.servo_zero_offset[kServoHead] = -5;
  settings_.servo_zero_offset[kServoLeftFrontShoulder] = 7;
  settings_.servo_zero_offset[kServoRightFrontShoulder] = 7;
   const int8_t* frame =
    animator_.GetFrame(kAnimationCalibrationPose, 0); 
  animator_.Attach();
  animator_.StartFrame(frame, 1);
  animator_.Animate(10000);
  for (int i = 0; i < kServoCount; ++i) {
    EXPECT_TRUE(animator_.servo_[i]->attached);
    int expected = 90;
    switch (i) {
     case kServoHead:
      expected = 85;
      break;

     case kServoLeftFrontShoulder:
      expected = 97;
      break;

     case kServoRightFrontShoulder:
      expected = 83;
      break;
    }
    EXPECT_EQ(expected, animator_.servo_[i]->value) << "servo " << i;
  }
}

void ServoAnimatorTest::TestAnimate(int servo, int* test_ms, int* expected_angle, int count) {
  for (int i = 0; i < count; ++i) {
    animator_.Animate(test_ms[i]);
    ASSERT_EQ(expected_angle[i], animator_.servo_[servo]->value) << "Test at " << test_ms[i] << "ms";
    ASSERT_EQ(i != count - 1, animator_.animating()) << "Test at " << test_ms[i] << "ms";
  }
}

TEST_F(ServoAnimatorTest, FrameInterpolationIsSmooth) {
  animator_.Attach();

  {
    animator_.StartFrame(animator_.GetFrame(kAnimationCalibrationPose, 0), 0);
    // From Rest to Calibrate Pose, the biggest change is shoulder rotation
    // from 60 to 0 degrees. With min_ms_per_angle_ of 1, this transition
    // should take 60 milliseconds to complete. Using cosine for smoothing, we
    // check progress at 1, 15, 30, 45, 59, and 60ms.

    int test_ms[] =        {   1,  15,  30,  45,  59,  60 };
    int expected_angle[] = { 150, 141, 120,  99,  90,  90 };

    TestAnimate(kServoLeftFrontShoulder, test_ms, expected_angle,
                sizeof(test_ms) / sizeof(test_ms[0]));
  }

  {
    // Move to balance will only take 30ms because biggest angle motion is
    // 30 degrees.
    animator_.StartFrame(animator_.GetFrame(kAnimationBalance, 0), 80);
    int test_ms[] =        {  81,  87,  95, 102, 109, 110 };
    int expected_angle[] = {  90,  86,  75,  65,  60,  60 };

    TestAnimate(kServoLeftFrontKnee, test_ms, expected_angle,
                sizeof(test_ms) / sizeof(test_ms[0]));
  }

  { // Animate back to rest. Track one of the negative direction motions.
    // Move from balance to rest moves knee servos 75 degrees (max movement).
    animator_.StartFrame(animator_.GetFrame(kAnimationRest, 0), 200);
    int test_ms[] =        { 201, 219, 238, 256, 274, 275 };
    int expected_angle[] = { 120, 109,  82,  56,  45,  45 };

    TestAnimate(kServoRightFrontKnee, test_ms, expected_angle,
                sizeof(test_ms) / sizeof(test_ms[0]));
  }
}

TEST_F(ServoAnimatorTest, AnimationCalibrationPoseCompletesAndStaysAttached) {
  animator_.StartAnimation(kAnimationCalibrationPose, 0);
  animator_.Animate(10000);
  EXPECT_EQ(90, animator_.servo_[kServoHead]->value);
  EXPECT_FALSE(animator_.animating());
  EXPECT_TRUE(animator_.servo_[kServoHead]->attached);
}

TEST_F(ServoAnimatorTest, AnimationRestCompletesAndDetaches) {
  animator_.StartAnimation(kAnimationRest, 0);

  animator_.Animate(10000);
  EXPECT_FALSE(animator_.animating());
  EXPECT_FALSE(animator_.servo_[kServoHead]->attached);
}

TEST_F(ServoAnimatorTest, AnimationWalkLoops) {
  unsigned long millis_now = 0;
  animator_.StartAnimation(kAnimationWalk, millis_now);

  for (int i = 0; i < 600; ++i) {
    millis_now += 1000;
    animator_.Animate(millis_now);
    ASSERT_TRUE(animator_.servo_[kServoHead]->attached);
    ASSERT_TRUE(animator_.animating());
    int next_frame = (i + 1) % 43;
    ASSERT_EQ(next_frame, animator_.animation_sequence_frame_number());
  }
}

TEST_F(ServoAnimatorTest, AnimationCalibrationPoseBalances) {
  animator_.StartAnimation(kAnimationCalibrationPose, 0);

  animator_.HandlePitchRoll(-10, 0, 0);
  animator_.Animate(10000);
  EXPECT_EQ(80, animator_.servo_[kServoHead]->value);
  EXPECT_EQ(90, animator_.servo_[kServoNeck]->value);

  animator_.HandlePitchRoll(0, 20, 10000);
  animator_.Animate(20000);
  EXPECT_EQ(90, animator_.servo_[kServoHead]->value);
  EXPECT_EQ(110, animator_.servo_[kServoNeck]->value);

  animator_.HandlePitchRoll(0, 0, 20000);
  animator_.Animate(30000);
  EXPECT_EQ(90, animator_.servo_[kServoHead]->value);
  EXPECT_EQ(90, animator_.servo_[kServoNeck]->value);
}

TEST_F(ServoAnimatorTest, ExtentsAreRespected) {
  settings_.servo_lower_extents[kServoHead] = -30;
  settings_.servo_upper_extents[kServoLeftFrontShoulder] = 40;
  animator_.Attach();

  int actual_rest_positions[kServoCount];
  memcpy(actual_rest_positions, rest_positions_, sizeof(actual_rest_positions));
  actual_rest_positions[kServoHead] = 60;
  actual_rest_positions[kServoLeftFrontShoulder] = 130;

  for (int i = 0; i < kServoCount; ++i) {
    EXPECT_TRUE(animator_.servo_[i]->attached);
    EXPECT_EQ(actual_rest_positions[i], animator_.servo_[i]->value);
  }
}
