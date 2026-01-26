/**
 * @file rollingstats.h
 * @brief Rolling window statistics utility for streaming diagnostics.
 *
 * Provides efficient calculation of mean, standard deviation, min, and max
 * over a fixed-size rolling window of samples.
 */

#ifndef ROLLINGSTATS_H
#define ROLLINGSTATS_H

#include <cmath>
#include <limits>
#include <vector>

/**
 * @brief Calculates rolling statistics over a fixed window of samples.
 *
 * Uses Welford's online algorithm for numerically stable variance calculation.
 * The window size is fixed at construction time.
 *
 * @par Example usage:
 * @code
 * RollingStats stats(100);  // 100-sample window
 * stats.addSample(10.5);
 * stats.addSample(11.2);
 * double avg = stats.mean();
 * double sd = stats.stddev();
 * @endcode
 */
class RollingStats
{
public:
    /**
     * @brief Constructs a RollingStats calculator with the specified window size.
     * @param windowSize Maximum number of samples to keep in the rolling window.
     */
    explicit RollingStats(size_t windowSize = 100)
        : windowSize_(windowSize)
    {
        samples_.reserve(windowSize);
    }

    /**
     * @brief Adds a sample to the rolling window.
     * @param value The sample value to add.
     *
     * If the window is full, the oldest sample is removed before adding
     * the new one. Statistics are updated incrementally for efficiency.
     */
    void addSample(double value)
    {
        if (samples_.size() >= windowSize_) {
            // Get old value before overwriting
            double oldValue = samples_[writeIndex_];

            // Overwrite in vector FIRST so recalculateMinMax sees correct values
            samples_[writeIndex_] = value;

            // Remove old sample from stats (may recalculate min/max from vector)
            removeSample(oldValue);

            // Add new sample to running statistics
            addToStats(value);
        } else {
            samples_.push_back(value);
            addToStats(value);
        }

        // Update write index for circular buffer
        writeIndex_ = (writeIndex_ + 1) % windowSize_;
    }

    /**
     * @brief Returns the mean of all samples in the window.
     * @return The mean value, or 0.0 if no samples.
     */
    [[nodiscard]] double mean() const
    {
        return count_ > 0 ? mean_ : 0.0;
    }

    /**
     * @brief Returns the sample standard deviation of all samples in the window.
     * @return The standard deviation, or 0.0 if fewer than 2 samples.
     */
    [[nodiscard]] double stddev() const
    {
        if (count_ < 2) {
            return 0.0;
        }
        return std::sqrt(m2_ / static_cast<double>(count_ - 1));
    }

    /**
     * @brief Returns the population standard deviation of all samples in the window.
     * @return The population standard deviation, or 0.0 if no samples.
     */
    [[nodiscard]] double stddevPopulation() const
    {
        if (count_ < 1) {
            return 0.0;
        }
        return std::sqrt(m2_ / static_cast<double>(count_));
    }

    /**
     * @brief Returns the minimum value in the window.
     * @return The minimum value, or positive infinity if no samples.
     */
    [[nodiscard]] double min() const
    {
        return min_;
    }

    /**
     * @brief Returns the maximum value in the window.
     * @return The maximum value, or negative infinity if no samples.
     */
    [[nodiscard]] double max() const
    {
        return max_;
    }

    /**
     * @brief Returns the number of samples currently in the window.
     * @return The sample count.
     */
    [[nodiscard]] size_t count() const
    {
        return count_;
    }

    /**
     * @brief Returns whether the window is full.
     * @return true if the window contains windowSize samples.
     */
    [[nodiscard]] bool isFull() const
    {
        return count_ >= windowSize_;
    }

    /**
     * @brief Clears all samples and resets statistics.
     */
    void clear()
    {
        samples_.clear();
        writeIndex_ = 0;
        count_ = 0;
        mean_ = 0.0;
        m2_ = 0.0;
        min_ = std::numeric_limits<double>::infinity();
        max_ = -std::numeric_limits<double>::infinity();
    }

    /**
     * @brief Returns the window size.
     * @return The maximum number of samples in the window.
     */
    [[nodiscard]] size_t windowSize() const
    {
        return windowSize_;
    }

private:
    void addToStats(double value)
    {
        count_++;
        double delta = value - mean_;
        mean_ += delta / static_cast<double>(count_);
        double delta2 = value - mean_;
        m2_ += delta * delta2;

        min_ = std::min(min_, value);
        max_ = std::max(max_, value);
    }

    void removeSample(double value)
    {
        if (count_ <= 1) {
            clear();
            return;
        }

        // Reverse Welford update
        double oldMean = mean_;
        mean_ = (mean_ * static_cast<double>(count_) - value) / static_cast<double>(count_ - 1);
        m2_ -= (value - mean_) * (value - oldMean);
        count_--;

        // Clamp m2_ to prevent negative variance due to floating point errors
        m2_ = std::max(m2_, 0.0);

        // Recalculate min/max if needed (expensive but rare)
        if (value <= min_ || value >= max_) {
            recalculateMinMax();
        }
    }

    void recalculateMinMax()
    {
        min_ = std::numeric_limits<double>::infinity();
        max_ = -std::numeric_limits<double>::infinity();

        for (double sample : samples_) {
            min_ = std::min(min_, sample);
            max_ = std::max(max_, sample);
        }
    }

    size_t windowSize_;
    std::vector<double> samples_;
    size_t writeIndex_ = 0;
    size_t count_ = 0;
    double mean_ = 0.0;
    double m2_ = 0.0;  // Sum of squared differences from mean (Welford)
    double min_ = std::numeric_limits<double>::infinity();
    double max_ = -std::numeric_limits<double>::infinity();
};

#endif // ROLLINGSTATS_H
