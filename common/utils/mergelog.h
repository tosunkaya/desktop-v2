#ifndef MERGELOG_H
#define MERGELOG_H

#include <QDateTime>
#include <QString>
#include <QMutex>
#include <vector>

// merge logs files log_gui.txt, log_engine.txt, windscribeservice.log to one, cut out the middle of the log if the count of lines  exceeds MAX_COUNT_OF_LINES
class MergeLog
{
public:
    static QString mergeLogs(bool doMergePerLine);
    static QString mergePrevLogs(bool doMergePerLine);

    // This is a quick hack to prevent GUI crash as result of merging files that are too large for the program
    static bool canMerge();
private:
    static constexpr int MAX_COUNT_OF_LINES = 100000;
    static QString merge(const QString &guiLogFilename, const QString &engineLogFilename,
                         const QString &serviceLogFilename, const QString &servicePrevLogFilename,
                         bool doMergePerLine);

    enum class LineSource { GUI, ENGINE, SERVICE, NUM_LINE_SOURCES };
    static int mergeTask(QMutex *mutex, QMultiMap<quint64, QPair<LineSource, QString>> *lines, const QString *filename, LineSource source, bool useMinMax, QDateTime min, QDateTime max);

    static const QString guiLogLocation();
    static const QString engineLogLocation();
    static const QString serviceLogLocation();

    static const QString prevGuiLogLocation();
    static const QString prevEngineLogLocation();
    static const QString prevServiceLogLocation();
};


#endif // MERGELOG_H
