// system includes
#include <iostream>
#include <deque>

// library includes
#include <QDebug>
#include <QObject>
#include <QTimer>

// local includes
#include "worker.h"
#include "shared.h"

namespace {

    enum OP_TYPE {
        UNINTEGRATE = 0,
    };

    typedef std::pair<QString, OP_TYPE> Operation;

}

namespace appimagelauncher::daemon {

    Q_LOGGING_CATEGORY(workerCat, "appimagelauncher.daemon.worker")

    class Worker::PrivateData {
    public:
        QTimer deferredOperationsTimer;

        static constexpr int TIMEOUT = 15 * 1000;

        // std::set is unordered, therefore using std::deque to keep the order of the operations
        std::deque<Operation> deferredOperations;

    public:
        PrivateData() {
            deferredOperationsTimer.setSingleShot(true);
            deferredOperationsTimer.setInterval(TIMEOUT);
        }

    public:
        // Ignore repeated removal notifications for the same path.
        bool isDuplicate(Operation operation) {
            for (auto it = deferredOperations.rbegin(); it != deferredOperations.rend(); ++it) {
                if ((*it).first == operation.first) {
                    // if operation type is different, then the operation is new, and should be added to the list
                    // if it is equal, it's a duplicate
                    // in either case, the loop can be aborted here
                    return (*it).second == operation.second;
                }
            }

            return false;
        }
    };

    Worker::Worker(QObject* parent) : QObject(parent) {
        d = std::make_shared<PrivateData>();

        connect(this, &Worker::startTimer, this, &Worker::startTimerIfNecessary, Qt::QueuedConnection);
        connect(&d->deferredOperationsTimer, &QTimer::timeout, this, &Worker::executeDeferredOperations);
    }

    void Worker::executeDeferredOperations() {
        if (d->deferredOperations.empty()) {
            qCDebug(workerCat) << "No deferred operations to execute";
            return;
        }

        std::cout << "Executing deferred operations" << std::endl;

        while (!d->deferredOperations.empty()) {
            auto operation = d->deferredOperations.front();
            d->deferredOperations.pop_front();
            if (operation.second == UNINTEGRATE)
                forgetDetectedAppImage(operation.first);
        }

        std::cout << "Cleaning up old desktop integration files" << std::endl;
        if (!cleanUpOldDesktopIntegrationResources(true)) {
            std::cout << "Failed to clean up old desktop integration files" << std::endl;
        }

        // make sure the icons in the launcher are refreshed
        std::cout << "Updating desktop database and icon caches" << std::endl;
        if (!updateDesktopDatabaseAndIconCaches())
            std::cout << "Failed to update desktop database and icon caches" << std::endl;

        std::cout << "Done" << std::endl;
    }

    void Worker::scheduleForUnintegration(const QString& path) {
        auto operation = std::make_pair(path, UNINTEGRATE);
        if (!d->isDuplicate(operation)) {
            std::cout << "Scheduling for unintegration: " << path.toStdString() << std::endl;
            d->deferredOperations.push_back(operation);
            emit startTimer();
        }
    }

    void Worker::startTimerIfNecessary() {
        if (!d->deferredOperationsTimer.isActive())
            QMetaObject::invokeMethod(&d->deferredOperationsTimer, "start");
    }

}
