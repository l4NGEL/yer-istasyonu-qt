#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "gcs/geo.hpp"
#include "gcs/latest_mailbox.hpp"

using namespace gcs;

TEST(LatestMailbox, NotifiesOnlyWhenEmpty) {
    LatestMailbox<int> mb;
    EXPECT_TRUE(mb.publish(1));
    EXPECT_FALSE(mb.publish(2));  // dolu kutuya yazmak uyarı üretmez, eskiyi ezer
    auto v = mb.take();
    ASSERT_TRUE(v);
    EXPECT_EQ(*v, 2);
    EXPECT_FALSE(mb.take());
    EXPECT_TRUE(mb.publish(3));
    const auto s = mb.stats();
    EXPECT_EQ(s.published, 3u);
    EXPECT_EQ(s.taken, 1u);
    EXPECT_EQ(s.overwritten, 1u);
}

// Üretici hızlı, tüketici yavaş: hiçbir değer iki kez alınmamalı, alınan değerler
// artan sırada olmalı ve published == taken + overwritten (+ kutuda kalan) olmalı.
// CI'da ThreadSanitizer altında da koşar.
TEST(LatestMailbox, ConcurrentProducerConsumerInvariants) {
    LatestMailbox<std::uint64_t> mb;
    constexpr std::uint64_t kCount = 200000;
    std::atomic<bool> done{false};
    std::vector<std::uint64_t> seen;
    std::mutex wakeMutex;
    std::condition_variable wake;
    std::atomic<std::uint64_t> notifications{0};

    std::thread consumer([&] {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(wakeMutex);
                wake.wait_for(lock, std::chrono::microseconds(200));
            }
            while (auto v = mb.take()) seen.push_back(*v);
            if (done.load()) {
                while (auto v = mb.take()) seen.push_back(*v);
                break;
            }
        }
    });
    std::thread producer([&] {
        for (std::uint64_t i = 1; i <= kCount; ++i) {
            if (mb.publish(i)) {
                notifications.fetch_add(1);
                wake.notify_one();
            }
        }
        done.store(true);
        wake.notify_one();
    });
    producer.join();
    consumer.join();

    ASSERT_FALSE(seen.empty());
    for (std::size_t i = 1; i < seen.size(); ++i) ASSERT_LT(seen[i - 1], seen[i]);
    EXPECT_EQ(seen.back(), kCount);  // son değer asla kaybolmaz
    const auto s = mb.stats();
    EXPECT_EQ(s.published, kCount);
    EXPECT_EQ(s.taken, seen.size());
    EXPECT_EQ(s.published, s.taken + s.overwritten);
    // Her uyarı bir boş→dolu geçişidir, her take() kutuyu boşaltır; sonda kutu
    // boş olduğuna göre iki sayı birebir eşit olmalı (kayıp uyandırma yok).
    EXPECT_EQ(notifications.load(), s.taken);
}

TEST(LatestMailbox, MultipleProducers) {
    LatestMailbox<std::unique_ptr<int>> mb;  // yalnız taşınabilir tip de çalışmalı
    std::vector<std::thread> producers;
    for (int t = 0; t < 4; ++t) {
        producers.emplace_back([&mb, t] {
            for (int i = 0; i < 20000; ++i) mb.publish(std::make_unique<int>(t * 100000 + i));
        });
    }
    std::uint64_t taken = 0;
    std::atomic<bool> stop{false};
    std::thread consumer([&] {
        while (!stop.load()) {
            if (mb.take()) ++taken;
        }
        while (mb.take()) ++taken;
    });
    for (auto& p : producers) p.join();
    stop.store(true);
    consumer.join();
    const auto s = mb.stats();
    EXPECT_EQ(s.published, 80000u);
    EXPECT_EQ(s.taken, taken);
    EXPECT_EQ(s.published, s.taken + s.overwritten);
}

TEST(Geo, OneDegreeLatitude) {
    // 1° enlem ≈ R·π/180 = 111 195 m (küresel yaklaşım)
    EXPECT_NEAR(geo::distanceM({39.0, 32.0}, {40.0, 32.0}), 111195.0, 1.0);
}

TEST(Geo, BearingCardinalDirections) {
    EXPECT_NEAR(geo::bearingDeg({39.0, 32.0}, {39.1, 32.0}), 0.0, 1e-6);
    EXPECT_NEAR(geo::bearingDeg({39.0, 32.0}, {39.0, 32.1}), 90.0, 0.05);
    EXPECT_NEAR(geo::bearingDeg({39.0, 32.0}, {38.9, 32.0}), 180.0, 1e-6);
}

TEST(Geo, DestinationRoundTrip) {
    const geo::LatLon a{39.8890, 32.7785};
    for (double brg : {0.0, 45.0, 137.0, 270.0}) {
        const auto b = geo::destination(a, brg, 1234.5);
        EXPECT_NEAR(geo::distanceM(a, b), 1234.5, 1e-3);
        EXPECT_NEAR(geo::bearingDeg(a, b), brg, 1e-3);
    }
}

TEST(Geo, Wrap) {
    EXPECT_DOUBLE_EQ(geo::wrap360(-10), 350);
    EXPECT_DOUBLE_EQ(geo::wrap360(725), 5);
    EXPECT_DOUBLE_EQ(geo::wrap180(190), -170);
    EXPECT_DOUBLE_EQ(geo::wrap180(-190), 170);
}

TEST(Geo, QuaternionRoundTrip) {
    for (const auto& e : {geo::EulerDeg{0, -45, 30}, geo::EulerDeg{10, 20, -170},
                          geo::EulerDeg{0, -89, 0}}) {
        const auto back = geo::eulerFromQuat(geo::quatFromEuler(e));
        EXPECT_NEAR(back.roll, e.roll, 1e-6);
        EXPECT_NEAR(back.pitch, e.pitch, 1e-6);
        EXPECT_NEAR(back.yaw, e.yaw, 1e-6);
    }
}
