#ifndef ANNOTATION_VISUALIZER_ANNOTATIONVISUALIZER_H
#define ANNOTATION_VISUALIZER_ANNOTATIONVISUALIZER_H

#include <QMainWindow>
#include <QImage>
#include <QCloseEvent>
#include <QDir>

QT_BEGIN_NAMESPACE
class QAction;
class QActionGroup;
class QLabel;
class QMenu;
class QScrollArea;
class QScrollBar;
QT_END_NAMESPACE

class AnnotationVisualizer : public QMainWindow
{
Q_OBJECT

public:
    explicit AnnotationVisualizer(QWidget *parent = nullptr);
    ~AnnotationVisualizer() override;

private slots:
    void open();
    void save();
    void closeImg();
    void chooseSegmentationMethod(QAction* chooseMethodAct);
    void chooseSPNumber(QAction* chooseSPNumberAct);
    void zoomIn();
    void zoomOut();
    void resetSize();
    void nextSlice();
    void previousSlice();
    void changeDisplayGrid();
    void hideAnnotations();
    void changeDisplayedAnnotations(QAction* changeDisplayedAnnotationsAct);
    void instructions();

private:
    void createActions();
    void addAnnotatorsActions();
    void updateActions();

    bool loadFiles(const QString &fileName);
    bool loadAnnotations(const QDir& currDir);
    bool loadRaw(const QString &fileName, unsigned short ***dataArray) const;
    bool loadRaw(const QString &fileName, char ***dataArray) const;
    bool loadRaw(const QString &fileName, bool ***dataArray) const;
    bool rescaleData(unsigned short ***dataArray) const;

    void updateDisplay();
    void scaleImage(double factor);
    static void adjustScrollBar(QScrollBar *scrollBar, double factor);

    unsigned short ***stirData;
    bool ***gridData;
    char ****spAnnotationData; // sp annotation is 0 (no lesion) or 1 (lesion)
    char ****manualCorrectionsData; // manual correction is -1 (remove from annotation), 0 (do nothing) or 1 (add to annotation)

    QStringList annotatorsList;

    QString loadedFileName = "";
    QString imageType;
    QString segmentationMethod = "LSC";
    QString spNumber = "LOWER";
    QString displayedAnnotations = "BOTH";

    int imageWidth {};
    int imageHeight {};
    int slicesNo {};
    int patientNo {};

    double scaleFactor = 1;
    int currSlice = 0;
    bool displayGrid = false;
    bool gridDataAvailable = false;
    bool annotationsHidden = false;

    QMenu *annotationsMenu;

    QLabel *imageLabel;
    QScrollArea *scrollArea;

    QAction *saveAct;
    QAction *closeImgAct;
    QActionGroup *segMethodChoiceGroup;
    QActionGroup *spNumberChoiceGroup;
    QAction *setLessSpAct;
    QAction *setMoreSpAct;
    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *normalSizeAct;
    QAction *nextSliceAct;
    QAction *previousSliceAct;
    QAction *displayGridAct;
    QAction *hideAnnotationsAct;
    QActionGroup *annotationsDisplayChoiceGroup;
    QActionGroup *annotatorsChoiceGroup;
};

#endif
