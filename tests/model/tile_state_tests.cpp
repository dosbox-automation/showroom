// This file is part of the dosbox-automation-showroom Project.
// License: GPL-3.0-or-later. Contact: dosbox-automation-showroom-project@trinity2k.net
//

#include "model/tile_state.h"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <utility>

namespace showroom {
namespace {

using Transition = std::pair<TileState, TileState>;

// The whole legal table, written out once. Every test below reads from
// this set rather than from the implementation, so an edge added to the
// production code without a decision here fails the exhaustive test.
const std::set<Transition>& legalTransitions()
{
    static const std::set<Transition> kTable = {
            // The user starts a download, or the network disappears first.
            {TileState::NotDownloaded, TileState::Downloading},
            {TileState::NotDownloaded, TileState::OfflineNotDownloaded},
            {TileState::OfflineNotDownloaded, TileState::NotDownloaded},

            // A download either arrives or it does not. Cancel and failure
            // both land back where they started.
            {TileState::Downloading, TileState::Downloaded},
            {TileState::Downloading, TileState::NotDownloaded},

            // Verification can reject an archive after the fact, which
            // discards it and puts the tile back on the download button.
            {TileState::Downloaded, TileState::NotDownloaded},
            {TileState::Downloaded, TileState::Installing},

            // An install that fails rolls back to the downloaded archive so
            // the retry does not have to fetch it again.
            {TileState::Installing, TileState::Ready},
            {TileState::Installing, TileState::Downloaded},

            {TileState::Ready, TileState::Running},
            {TileState::Running, TileState::Ready},

            // A damaged install rolls back for reinstallation: to the kept
            // archive when one is on disk, otherwise to the download button.
            {TileState::Ready, TileState::Downloaded},
            {TileState::Ready, TileState::NotDownloaded},
    };
    return kTable;
}

bool isLegalInTable(TileState from, TileState to)
{
    return legalTransitions().count(Transition{from, to}) > 0;
}

TEST(TileStateTransitions, table_matches_the_implementation_for_every_pair)
{
    for (const TileState from : kAllTileStates) {
        for (const TileState to : kAllTileStates) {
            EXPECT_EQ(isLegalTransition(from, to), isLegalInTable(from, to))
                    << "from " << tileStateName(from) << " to " << tileStateName(to);
        }
    }
}

TEST(TileStateTransitions, a_state_never_transitions_to_itself)
{
    for (const TileState state : kAllTileStates) {
        EXPECT_FALSE(isLegalTransition(state, state)) << tileStateName(state);
    }
}

TEST(TileStateTransitions, downloading_reaches_downloaded_or_not_downloaded_only)
{
    EXPECT_TRUE(isLegalTransition(TileState::Downloading, TileState::Downloaded));
    EXPECT_TRUE(isLegalTransition(TileState::Downloading, TileState::NotDownloaded));

    // Downloading is not installing: an arriving archive never launches
    // anything by itself.
    EXPECT_FALSE(isLegalTransition(TileState::Downloading, TileState::Running));
    EXPECT_FALSE(isLegalTransition(TileState::Downloading, TileState::Ready));
    EXPECT_FALSE(isLegalTransition(TileState::Downloading, TileState::Installing));
}

TEST(TileStateTransitions, installing_reaches_ready_or_rolls_back_to_downloaded)
{
    EXPECT_TRUE(isLegalTransition(TileState::Installing, TileState::Ready));
    EXPECT_TRUE(isLegalTransition(TileState::Installing, TileState::Downloaded));

    EXPECT_FALSE(isLegalTransition(TileState::Installing, TileState::Running));
    EXPECT_FALSE(isLegalTransition(TileState::Installing, TileState::NotDownloaded));
}

TEST(TileStateTransitions, running_returns_to_ready_when_the_process_exits)
{
    EXPECT_TRUE(isLegalTransition(TileState::Ready, TileState::Running));
    EXPECT_TRUE(isLegalTransition(TileState::Running, TileState::Ready));

    EXPECT_FALSE(isLegalTransition(TileState::Running, TileState::NotDownloaded));
    EXPECT_FALSE(isLegalTransition(TileState::Running, TileState::Downloaded));
}

TEST(TileStateTransitions, a_damaged_ready_install_rolls_back_for_reinstall)
{
    EXPECT_TRUE(isLegalTransition(TileState::Ready, TileState::Downloaded));
    EXPECT_TRUE(isLegalTransition(TileState::Ready, TileState::NotDownloaded));

    // Damage never skips stations: a Ready tile does not jump into a
    // running download or install by itself.
    EXPECT_FALSE(isLegalTransition(TileState::Ready, TileState::Downloading));
    EXPECT_FALSE(isLegalTransition(TileState::Ready, TileState::Installing));
}

TEST(TileStateTransitions, connectivity_moves_only_the_not_downloaded_state)
{
    EXPECT_TRUE(
            isLegalTransition(TileState::NotDownloaded, TileState::OfflineNotDownloaded));
    EXPECT_TRUE(
            isLegalTransition(TileState::OfflineNotDownloaded, TileState::NotDownloaded));

    // A game already on disk stays usable without a network, so no other
    // state has an offline counterpart to move to.
    EXPECT_FALSE(
            isLegalTransition(TileState::Downloaded, TileState::OfflineNotDownloaded));
    EXPECT_FALSE(isLegalTransition(TileState::Ready, TileState::OfflineNotDownloaded));
    EXPECT_FALSE(
            isLegalTransition(TileState::OfflineNotDownloaded, TileState::Downloading));
}

TEST(TileStateTransitions, the_no_recipe_state_is_a_dead_end_in_both_directions)
{
    // It describes the definition, not the progress of a download, so
    // nothing enters or leaves it while the application runs.
    for (const TileState state : kAllTileStates) {
        EXPECT_FALSE(isLegalTransition(TileState::NoRecipe, state))
                << tileStateName(state);
        EXPECT_FALSE(isLegalTransition(state, TileState::NoRecipe))
                << tileStateName(state);
    }
}

TEST(TileStateActions, each_state_offers_the_button_the_design_specifies)
{
    EXPECT_EQ(actionFor(TileState::NotDownloaded), TileAction::Download);
    EXPECT_EQ(actionFor(TileState::Downloaded), TileAction::Play);
    EXPECT_EQ(actionFor(TileState::Ready), TileAction::Play);
    EXPECT_EQ(actionFor(TileState::Running), TileAction::Stop);

    // Work in progress and the two dead ends offer nothing to click.
    EXPECT_EQ(actionFor(TileState::Downloading), TileAction::None);
    EXPECT_EQ(actionFor(TileState::Installing), TileAction::None);
    EXPECT_EQ(actionFor(TileState::OfflineNotDownloaded), TileAction::None);
    EXPECT_EQ(actionFor(TileState::NoRecipe), TileAction::None);
}

TEST(TileStateTones, the_legend_colour_follows_the_state)
{
    EXPECT_EQ(toneFor(TileState::NotDownloaded), TileTone::Idle);
    EXPECT_EQ(toneFor(TileState::Downloaded), TileTone::Idle);

    EXPECT_EQ(toneFor(TileState::Downloading), TileTone::Working);
    EXPECT_EQ(toneFor(TileState::Installing), TileTone::Working);

    EXPECT_EQ(toneFor(TileState::Ready), TileTone::Ready);
    EXPECT_EQ(toneFor(TileState::Running), TileTone::Ready);

    EXPECT_EQ(toneFor(TileState::OfflineNotDownloaded), TileTone::Disabled);
    EXPECT_EQ(toneFor(TileState::NoRecipe), TileTone::Disabled);
}

TEST(TileStateTones, only_the_working_states_show_progress)
{
    EXPECT_TRUE(showsProgress(TileState::Downloading));
    EXPECT_TRUE(showsProgress(TileState::Installing));

    EXPECT_FALSE(showsProgress(TileState::NotDownloaded));
    EXPECT_FALSE(showsProgress(TileState::Downloaded));
    EXPECT_FALSE(showsProgress(TileState::Ready));
    EXPECT_FALSE(showsProgress(TileState::Running));
    EXPECT_FALSE(showsProgress(TileState::OfflineNotDownloaded));
    EXPECT_FALSE(showsProgress(TileState::NoRecipe));
}

TEST(TileStateNames, every_state_has_a_distinct_non_empty_name)
{
    std::set<std::string> seen;
    for (const TileState state : kAllTileStates) {
        const std::string name = tileStateName(state);
        EXPECT_FALSE(name.empty());
        EXPECT_TRUE(seen.insert(name).second) << "duplicate name: " << name;
    }
    EXPECT_EQ(seen.size(), kAllTileStates.size());
}

TEST(TileStateNames, an_out_of_range_value_names_itself_instead_of_reading_a_table)
{
    // The state can arrive from a settings file or a cast, and a log line
    // is the last place that should be allowed to walk off an array.
    EXPECT_STREQ(tileStateName(static_cast<TileState>(99)), "unknown");
}

} // namespace
} // namespace showroom
