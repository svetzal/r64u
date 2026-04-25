#include "services/servicefactory.h"

#include "services/deviceconnection.h"
#include "services/errorhandler.h"
#include "services/favoritesmanager.h"
#include "services/filepreviewservice.h"
#include "services/gamebase64service.h"
#include "services/httpfiledownloader.h"
#include "services/hvscmetadataservice.h"
#include "services/playlistmanager.h"
#include "services/songlengthsdatabase.h"
#include "services/statusmessageservice.h"
#include "services/transferservice.h"
#include "services/configfileloader.h"
#include "models/remotefilemodel.h"
#include "models/transferqueue.h"
#include "services/metadataservicebundle.h"

#include <QtTest>

class TestServiceFactory : public QObject
{
    Q_OBJECT

private:
    ServiceFactory *factory_ = nullptr;

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_servicefactory");

        factory_ = new ServiceFactory(nullptr, this);
    }

    void cleanup()
    {
        delete factory_;
        factory_ = nullptr;
    }

    void testAllServicesNonNull()
    {
        QVERIFY(factory_->deviceConnection() != nullptr);
        QVERIFY(factory_->remoteFileModel() != nullptr);
        QVERIFY(factory_->transferQueue() != nullptr);
        QVERIFY(factory_->configFileLoader() != nullptr);
        QVERIFY(factory_->filePreviewService() != nullptr);
        QVERIFY(factory_->transferService() != nullptr);
        QVERIFY(factory_->errorHandler() != nullptr);
        QVERIFY(factory_->statusMessageService() != nullptr);
        QVERIFY(factory_->favoritesManager() != nullptr);
        QVERIFY(factory_->playlistManager() != nullptr);
        QVERIFY(factory_->songlengthsDownloader() != nullptr);
        QVERIFY(factory_->songlengthsDatabase() != nullptr);
        QVERIFY(factory_->stilDownloader() != nullptr);
        QVERIFY(factory_->buglistDownloader() != nullptr);
        QVERIFY(factory_->hvscMetadataService() != nullptr);
        QVERIFY(factory_->gameBase64Downloader() != nullptr);
        QVERIFY(factory_->gameBase64Service() != nullptr);
    }

    void testMetadataBundleHasAllComponents()
    {
        MetadataServiceBundle bundle = factory_->metadataBundle();
        QVERIFY(bundle.songlengthsDownloader != nullptr);
        QVERIFY(bundle.songlengthsDatabase != nullptr);
        QVERIFY(bundle.stilDownloader != nullptr);
        QVERIFY(bundle.buglistDownloader != nullptr);
        QVERIFY(bundle.hvscMetadataService != nullptr);
        QVERIFY(bundle.gameBase64Downloader != nullptr);
        QVERIFY(bundle.gameBase64Service != nullptr);
    }

    void testDownloadersAreDistinctInstances()
    {
        QVERIFY(factory_->songlengthsDownloader() != factory_->stilDownloader());
        QVERIFY(factory_->stilDownloader() != factory_->buglistDownloader());
        QVERIFY(factory_->buglistDownloader() != factory_->gameBase64Downloader());
    }

    void testServicesHaveFactoryAsParent()
    {
        QCOMPARE(factory_->deviceConnection()->parent(), factory_);
        QCOMPARE(factory_->errorHandler()->parent(), factory_);
        QCOMPARE(factory_->statusMessageService()->parent(), factory_);
    }
};

QTEST_MAIN(TestServiceFactory)
#include "test_servicefactory.moc"
