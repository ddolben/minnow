#include "common/test/test.h"
#include "cartridge.h"

using dgb::MBC3RTC;
using RTCData = dgb::MBC3RTC::RTCData;

TEST(MBC3RTC_RTCDataToTimestamp_HandlesBasicCase) {
  RTCData rtc;
  rtc.seconds = 36;
  rtc.minutes = 24;
  rtc.hours = 12;
  rtc.days = 147;

  uint64_t timestamp = MBC3RTC::RTCDataToTimestamp(rtc);
  uint64_t expected_timestamp = 36 + (24*60) + (12*3600) + (147*86400);
  ASSERT_EQ(expected_timestamp, timestamp);

  return 0;
}

TEST(MBC3RTC_TimestampToRTCData_HandlesBasicCase) {
  uint64_t timestamp = 36 + (24*60) + (12*3600) + (147*86400);
  RTCData got = MBC3RTC::TimestampToRTCData(timestamp);

  ASSERT_EQ(36, got.seconds);
  ASSERT_EQ(24, got.minutes);
  ASSERT_EQ(12, got.hours);
  ASSERT_EQ(147, got.days);

  return 0;
}

TEST(MBC3RTC_TimestampToRTCData_HandlesDayOverflow) {
  uint64_t timestamp = 36 + (24*60) + (12*3600) + (513*86400);
  RTCData got = MBC3RTC::TimestampToRTCData(timestamp);

  ASSERT_EQ(36, got.seconds);
  ASSERT_EQ(24, got.minutes);
  ASSERT_EQ(12, got.hours);
  ASSERT_EQ(1 | MBC3RTC::RTC_DAY_OVERFLOW_BIT, got.days);

  return 0;
}

TEST(MBC3RTC_ComputeNewRTCData_HandlesBasicCase) {
  RTCData rtc;
  rtc.seconds = 36;
  rtc.minutes = 24;
  rtc.hours = 12;
  rtc.days = 147;

  uint64_t delta = 1 + (2*60) + (3*3600) + (4*86400);
  RTCData got = MBC3RTC::ComputeNewRTCData(rtc, delta);

  ASSERT_EQ(37, got.seconds);
  ASSERT_EQ(26, got.minutes);
  ASSERT_EQ(15, got.hours);
  ASSERT_EQ(151, got.days);

  return 0;
}

TEST(MBC3RTC_ComputeNewRTCData_HandlesDayOverflow) {
  RTCData rtc;
  rtc.seconds = 36;
  rtc.minutes = 24;
  rtc.hours = 12;
  rtc.days = 147;

  uint64_t delta = 1 + (2*60) + (3*3600) + (513*86400);
  RTCData got = MBC3RTC::ComputeNewRTCData(rtc, delta);

  ASSERT_EQ(37, got.seconds);
  ASSERT_EQ(26, got.minutes);
  ASSERT_EQ(15, got.hours);
  ASSERT_EQ(148 | MBC3RTC::RTC_DAY_OVERFLOW_BIT, got.days);

  return 0;
}
