// Copyright (c) 2017 - 2020 Settlers Freaks (sf-team at siedler25.org)
//
// This file is part of Return To The Roots.
//
// Return To The Roots is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// Return To The Roots is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Return To The Roots. If not, see <http://www.gnu.org/licenses/>.

#include "gameTypes/GameTypesOutput.h"
#include "gameTypes/Resource.h"
#include "gameData/JobConsts.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(GameTypes)

BOOST_AUTO_TEST_CASE(ResourceValues)
{
    // Basic value
    Resource res(ResourceType::Gold, 10);
    BOOST_REQUIRE_EQUAL(res.getType(), ResourceType::Gold);
    BOOST_REQUIRE_EQUAL(res.getAmount(), 10u);
    // Change type
    res.setType(ResourceType::Iron);
    BOOST_REQUIRE_EQUAL(res.getType(), ResourceType::Iron);
    BOOST_REQUIRE_EQUAL(res.getAmount(), 10u);
    // Amount
    res.setAmount(5);
    BOOST_REQUIRE_EQUAL(res.getType(), ResourceType::Iron);
    BOOST_REQUIRE_EQUAL(res.getAmount(), 5u);
    // Copy value
    Resource res2(res.getValue());
    BOOST_REQUIRE_EQUAL(res.getType(), ResourceType::Iron);
    BOOST_REQUIRE_EQUAL(res.getAmount(), 5u);
    // Set 0
    res2.setAmount(0);
    BOOST_REQUIRE_EQUAL(res2.getType(), ResourceType::Iron);
    BOOST_REQUIRE_EQUAL(res2.getAmount(), 0u);
    // Has
    BOOST_REQUIRE(res.has(ResourceType::Iron));
    BOOST_REQUIRE(!res.has(ResourceType::Gold));
    BOOST_REQUIRE(!res2.has(ResourceType::Iron));
    BOOST_REQUIRE(!res.has(ResourceType::Nothing));
    BOOST_REQUIRE(!res2.has(ResourceType::Nothing));
    // Nothing -> 0
    BOOST_REQUIRE_NE(res.getAmount(), 0u);
    res.setType(ResourceType::Nothing);
    BOOST_REQUIRE_EQUAL(res.getType(), ResourceType::Nothing);
    BOOST_REQUIRE_EQUAL(res.getAmount(), 0u);
    // And stays 0
    res.setAmount(10);
    BOOST_REQUIRE_EQUAL(res.getType(), ResourceType::Nothing);
    BOOST_REQUIRE_EQUAL(res.getAmount(), 0u);
    BOOST_REQUIRE(!res.has(ResourceType::Iron));
    BOOST_REQUIRE(!res.has(ResourceType::Nothing));
    // Overflow check
    res2.setAmount(15);
    BOOST_REQUIRE_EQUAL(res2.getType(), ResourceType::Iron);
    BOOST_REQUIRE_EQUAL(res2.getAmount(), 15u);
    res2.setAmount(17);
    BOOST_REQUIRE_EQUAL(res2.getType(), ResourceType::Iron);
    // Unspecified
    BOOST_REQUIRE_LT(res2.getAmount(), 17u);
}

BOOST_AUTO_TEST_CASE(NationSpecificJobBobs)
{
    // Helper is not nation specific
    BOOST_TEST(JOB_SPRITE_CONSTS[Job::Helper].getBobId(Nation::Vikings)
               == JOB_SPRITE_CONSTS[Job::Helper].getBobId(Nation::Africans));
    BOOST_TEST(JOB_SPRITE_CONSTS[Job::Helper].getBobId(Nation::Vikings)
               == JOB_SPRITE_CONSTS[Job::Helper].getBobId(Nation::Babylonians));
    // Soldiers are
    BOOST_TEST(JOB_SPRITE_CONSTS[Job::Private].getBobId(Nation::Vikings)
               != JOB_SPRITE_CONSTS[Job::Private].getBobId(Nation::Africans));
    // Non native nations come after native ones
    BOOST_TEST(JOB_SPRITE_CONSTS[Job::Private].getBobId(Nation::Vikings)
               < JOB_SPRITE_CONSTS[Job::Private].getBobId(Nation::Babylonians));
    // Same for scouts
    BOOST_TEST(JOB_SPRITE_CONSTS[Job::Scout].getBobId(Nation::Vikings)
               != JOB_SPRITE_CONSTS[Job::Scout].getBobId(Nation::Africans));
    BOOST_TEST(JOB_SPRITE_CONSTS[Job::Scout].getBobId(Nation::Vikings)
               < JOB_SPRITE_CONSTS[Job::Scout].getBobId(Nation::Babylonians));
}

BOOST_AUTO_TEST_SUITE_END()
