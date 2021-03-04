#ifndef ANNOTATIONMANAGER_H
#define ANNOTATIONMANAGER_H

#include <QMainWindow>
#include <QImage>
#include <QCloseEvent>

QT_BEGIN_NAMESPACE
class QAction;
class QActionGroup;
class QLabel;
class QMenu;
class QScrollArea;
class QScrollBar;
QT_END_NAMESPACE

class AnnotationManager : public QMainWindow
{
Q_OBJECT

public:
    explicit AnnotationManager(QWidget *parent = nullptr);
    ~AnnotationManager() override;

private slots:
    void open();
    void save();
    void closeImg();
    void chooseSegmentationMethod(QAction* chooseMethodAct);
    void chooseSPNumber(QAction* chooseSPNumberAct);
    void resetAnnotations();
    void changeAnnotationMode();
    void increaseManualPenSize();
    void reduceManualPenSize();
    void zoomIn();
    void zoomOut();
    void resetSize();
    void nextSlice();
    void previousSlice();
    void changeDisplayGrid();
    void changeDisplayAnnotations();
    void instructions();
    // TODO findMissingAnnotations() - method to find numbers of patients for which annotations have not been done yet

private:
    void createActions();
    void updateActions();

    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void manualCorrectionLine(QPoint &endPoint, const bool &adding);
    void markPixel(const int &x, const int &y, const bool &adding);
    void markSuperPixel(const QPoint & position, const bool &adding);

    bool loadFiles(const QString &fileName);
    bool loadRaw(const QString &fileName, unsigned short ***dataArray) const;
    bool loadRaw(const QString &fileName, char ***dataArray) const;
    bool loadRaw(const QString &fileName, bool ***dataArray) const;
    bool saveRaw(const QString &fileName, char ***dataArray) const;

    void updateDisplay();
    void scaleImage(double factor);
    static void adjustScrollBar(QScrollBar *scrollBar, double factor);

    unsigned short ***stirData;
    unsigned short ***spData;
    bool ***gridData;
    char ***spAnnotationData; // sp annotation is 0 (no lesion) or 1 (lesion)
    char ***manualCorrectionsData; // manual correction is -1 (remove from annotation), 0 (do nothing) or 1 (add to annotation)

    QString spAnnFileName;
    QString manualCorrFileName;
    QString loadedFileName = "";
    QString imageType;
    QString segmentationMethod = "LSC";
    QString spNumber = "LOWER";

    int imageWidth {};
    int imageHeight {};
    int slicesNo {};

    double scaleFactor = 1;
    int currSlice = 0;
    bool displayGrid = false;
    bool displayAnnotations = true;
    bool manualCorrectionsMode = false;

    bool unsavedChanges = false;
    bool clickedLeft = false;
    bool clickedRight = false;

    int manualPenSize = 3;
    QPoint lastManualPoint;

    QLabel *imageLabel;
    QScrollArea *scrollArea;

    QAction *saveAct;
    QAction *closeImgAct;
    QActionGroup *segMethodChoiceGroup;
    QActionGroup *spNumberChoiceGroup;
    QAction *setLessSpAct;
    QAction *setMoreSpAct;
    QAction *resetAnnotationsAct;
    QAction *changeAnnotationsModeAct;
    QAction *increaseManualPenSizeAct;
    QAction *reduceManualPenSizeAct;
    QAction *zoomInAct;
    QAction *zoomOutAct;
    QAction *normalSizeAct;
    QAction *nextSliceAct;
    QAction *previousSliceAct;
    QAction *displayGridAct;
    QAction *displayAnnotationsAct;
};

#endif
