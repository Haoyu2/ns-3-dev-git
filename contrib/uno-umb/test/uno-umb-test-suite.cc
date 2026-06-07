/*
 * Copyright (c) 2026 Haoyu
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/cell-sleep-controller.h"
#include "ns3/test.h"

using namespace ns3;

/**
 * @defgroup uno-umb-tests Tests for UNO-UMB
 * @ingroup uno-umb
 * @ingroup tests
 */

/**
 * @ingroup uno-umb-tests
 * Unit tests for cell sleep policy helpers.
 */
class CellSleepPolicyTestCase : public TestCase
{
  public:
    /**
     * Constructor.
     */
    CellSleepPolicyTestCase();

  private:
    void DoRun() override;
};

CellSleepPolicyTestCase::CellSleepPolicyTestCase()
    : TestCase("Validate cell sleep policy parsing and CSV header")
{
}

void
CellSleepPolicyTestCase::DoRun()
{
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(ParseCellSleepPolicy("all-on")),
                          static_cast<int>(CellSleepPolicyMode::AllOn),
                          "all-on policy parsing failed");
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(ParseCellSleepPolicy("threshold")),
                          static_cast<int>(CellSleepPolicyMode::Threshold),
                          "threshold policy parsing failed");
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(ParseCellSleepPolicy("aggressive")),
                          static_cast<int>(CellSleepPolicyMode::Aggressive),
                          "aggressive policy parsing failed");
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(ParseCellSleepPolicy("twin")),
                          static_cast<int>(CellSleepPolicyMode::Twin),
                          "twin policy parsing failed");

    const std::string header = CellSleepController::GetEventCsvHeader();
    NS_TEST_ASSERT_MSG_NE(header.find("twin_safe"),
                          std::string::npos,
                          "event CSV header should include twin_safe");
}

/**
 * @ingroup uno-umb-tests
 * Test suite for the UNO-UMB module.
 */
class UnoUmbTestSuite : public TestSuite
{
  public:
    /**
     * Constructor.
     */
    UnoUmbTestSuite();
};

UnoUmbTestSuite::UnoUmbTestSuite()
    : TestSuite("uno-umb", Type::UNIT)
{
    AddTestCase(new CellSleepPolicyTestCase, TestCase::Duration::QUICK);
}

/**
 * Static variable for test initialization.
 */
static UnoUmbTestSuite g_unoUmbTestSuite;
