#pragma once

#include <cmath>
#include <string>

// A topology-stable location on a course rail. normalizedT is local to the
// referenced segment; the three offsets are expressed in that segment's frame.
// Runtime distance is deliberately not stored here because it changes whenever
// an upstream control point is edited.
struct RailAnchor {
    std::string segmentGuid;
    float normalizedT = 0.0f;
    float lateralOffset = 0.0f;
    float verticalOffset = 0.0f;
    float forwardOffset = 0.0f;

    bool IsFinite() const noexcept {
        return std::isfinite(normalizedT) &&
            std::isfinite(lateralOffset) &&
            std::isfinite(verticalOffset) &&
            std::isfinite(forwardOffset);
    }
};

// The owner is the persistent editor GUID of a course event, camera key,
// terrain placement, rock cluster, or a future authored course object.
struct CourseRailAnchorBinding {
    std::string ownerGuid;
    RailAnchor anchor{};
};
