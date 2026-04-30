#ifndef EXPLOREFAVORITESCONTROLLER_H
#define EXPLOREFAVORITESCONTROLLER_H

#include <QObject>
#include <QString>

class FavoritesService;
class QAction;
class QMenu;

class ExploreFavoritesController : public QObject
{
    Q_OBJECT

public:
    explicit ExploreFavoritesController(FavoritesService *favoritesService,
                                        QObject *parent = nullptr);

    void setToggleAction(QAction *action);
    void setFavoritesMenu(QMenu *menu);
    void updateForPath(const QString &path);

    [[nodiscard]] bool isFavorite(const QString &path) const;

public slots:
    void onToggleFavorite(const QString &path);
    void onFavoriteSelected(QAction *action);
    void onFavoritesChanged();

signals:
    void navigateToPath(const QString &path);
    void statusMessage(const QString &message, int timeout = 0);

private:
    FavoritesService *favoritesService_ = nullptr;
    QAction *toggleFavoriteAction_ = nullptr;
    QMenu *favoritesMenu_ = nullptr;
};

#endif  // EXPLOREFAVORITESCONTROLLER_H
