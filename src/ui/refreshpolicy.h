#ifndef REFRESHPOLICY_H
#define REFRESHPOLICY_H

#include <QObject>

class RemoteFileModel;

/**
 * @brief Manages auto-refresh staleness tracking for a remote file browser.
 *
 * Encapsulates the suppress flag, RAII suppressor, and the
 * "refresh if stale when visible" logic that was previously embedded in
 * RemoteFileBrowserWidget.
 */
class RefreshPolicy : public QObject
{
    Q_OBJECT

public:
    explicit RefreshPolicy(QObject *parent = nullptr);

    void setModel(RemoteFileModel *model);
    void setConnected(bool connected);

    /**
     * @brief Suppresses auto-refresh while active; re-enables on destruction.
     */
    class Suppressor
    {
    public:
        explicit Suppressor(RefreshPolicy *policy);
        ~Suppressor();
        Suppressor(const Suppressor &) = delete;
        Suppressor &operator=(const Suppressor &) = delete;

    private:
        RefreshPolicy *policy_;
    };

    /**
     * @brief Returns a scoped suppressor — auto-refresh is re-enabled when it
     *        goes out of scope.
     */
    [[nodiscard]] Suppressor suppress();

    /**
     * @brief Sets the suppress flag directly (used by AutoRefreshSuppressor).
     */
    void setSuppressed(bool suppressed);

    /**
     * @brief Returns true if auto-refresh is currently suppressed.
     */
    [[nodiscard]] bool isSuppressed() const { return suppressed_; }

    /**
     * @brief Triggers a model refresh if not connected or suppressed.
     *
     * Calls RemoteFileModel::refreshIfStale() when conditions allow.
     */
    void refreshIfStale();

private:
    RemoteFileModel *model_ = nullptr;
    bool connected_ = false;
    bool suppressed_ = false;
};

#endif  // REFRESHPOLICY_H
