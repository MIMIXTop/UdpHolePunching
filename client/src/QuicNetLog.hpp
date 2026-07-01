#pragma once

#include <msquic.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <print>
#include <string_view>

namespace P2P::QuicNetLog {
    inline void Event(std::string_view level, std::string_view role, std::string_view event, std::string_view message) {
        if (level == "ERROR" || level == "WARN") {
            std::println(std::cerr, "[NET][QUIC][{}][{}][{}] {}", level, role, event, message);
            return;
        }

        std::println("[NET][QUIC][{}][{}][{}] {}", level, role, event, message);
    }

    inline void PacketLoss(std::string_view role, std::string_view trigger, const MsQuicConnection& connection) {
        QUIC_STATISTICS_V2 stats{};
        const QUIC_STATUS status = connection.GetStatistics(&stats);
        if (QUIC_FAILED(status)) {
            std::println(std::cerr, "[NET][QUIC][ERROR][{}][{}] failed to read statistics: 0x{:x}", role, trigger, status);
            return;
        }

        const uint64_t actualLost = stats.SendSuspectedLostPackets >= stats.SendSpuriousLostPackets
                                        ? (stats.SendSuspectedLostPackets - stats.SendSpuriousLostPackets)
                                        : 0;
        const uint64_t totalPackets = std::max<uint64_t>(stats.SendTotalPackets, 1);
        const double lossRatePercent =
            static_cast<double>(actualLost) * 100.0 / static_cast<double>(totalPackets);

        const char* severity = lossRatePercent >= 5.0 ? "ERROR" : (lossRatePercent >= 1.0 ? "WARN" : "INFO");
        std::println(
            severity == std::string_view("INFO") ? stdout : stderr,
            "[NET][QUIC][{}][{}][{}] loss={:.2f}% lost={} suspected={} spurious={} sent={} rtt_us={} cwnd={} cong={} pcong={}",
            severity,
            role,
            trigger,
            lossRatePercent,
            actualLost,
            stats.SendSuspectedLostPackets,
            stats.SendSpuriousLostPackets,
            stats.SendTotalPackets,
            stats.Rtt,
            stats.SendCongestionWindow,
            stats.SendCongestionCount,
            stats.SendPersistentCongestionCount
        );
    }
} // namespace P2P::QuicNetLog
