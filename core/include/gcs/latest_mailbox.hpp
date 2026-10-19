#pragma once
// Üretici iş parçacığından tüketiciye "yalnızca en son değer" aktarımı.
//
// Video kod çözücü saniyede 30+ kare üretirken arayüz yavaşlarsa kuyruk büyümesin
// diye eski kare yenisiyle ezilir ve sayılır. publish() yalnızca kutu boşken true
// döner; üretici sadece o zaman tüketiciyi uyarır. Uyarı ve kutu durumu aynı
// kilit altında değiştiği için uyandırma kaybı olmaz ve olay kuyruğu kare başına
// bir sinyalle dolmaz.

#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>

namespace gcs {

struct MailboxStats {
    std::uint64_t published = 0;
    std::uint64_t taken = 0;
    std::uint64_t overwritten = 0;  // tüketici almadan ezilen değerler
};

template <class T>
class LatestMailbox {
public:
    // true: kutu boştu, tüketicinin uyarılması gerekiyor.
    bool publish(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool wasEmpty = !slot_.has_value();
        if (!wasEmpty) ++stats_.overwritten;
        slot_ = std::move(value);
        ++stats_.published;
        return wasEmpty;
    }

    std::optional<T> take() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!slot_) return std::nullopt;
        std::optional<T> out = std::move(slot_);
        slot_.reset();
        ++stats_.taken;
        return out;
    }

    MailboxStats stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    bool hasValue() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return slot_.has_value();
    }

private:
    mutable std::mutex mutex_;
    std::optional<T> slot_;
    MailboxStats stats_;
};

}  // namespace gcs
