#include "annotationmanager.h"

#include <QApplication>
#include <QClipboard>
#include <QColorSpace>
#include <QDir>
#include <QFileDialog>
#include <QImageReader>
#include <QImageWriter>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSplitter>

#include <iostream>
#include <fstream>
#include <queue>
#include <utility>
#include <cmath>


AnnotationManager::AnnotationManager(QWidget *parent)
        : QMainWindow(parent), imageLabel(new QLabel), comparisonImageLabel(new QLabel)
        , scrollArea(new QScrollArea), comparisonScrollArea(new QScrollArea), splitter(new QSplitter)
{
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents(true);

    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(imageLabel);
    scrollArea->setVisible(false);

    splitter->addWidget(scrollArea);

    comparisonImageLabel->setBackgroundRole(QPalette::Base);
    comparisonImageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    comparisonImageLabel->setScaledContents(true);

    comparisonScrollArea->setBackgroundRole(QPalette::Dark);
    comparisonScrollArea->setWidget(comparisonImageLabel);
    comparisonScrollArea->setVisible(false);

    splitter->addWidget(comparisonScrollArea);

    setCentralWidget(splitter);

    createActions();

    resize(QGuiApplication::primaryScreen()->availableSize() * 3 / 5);
}

AnnotationManager::~AnnotationManager() {
    if (imageHeight != 0 && imageWidth != 0 && slicesNo != 0) {  // check if any file was ever loaded
        for (int i = 0; i < slicesNo; i++) {
            for (int j = 0; j < imageWidth; j++) {
                delete[] stirData[i][j];
            }
            delete[] stirData[i];
        }
        delete[] stirData;

        for (int i = 0; i < slicesNo; i++) {
            for (int j = 0; j < imageWidth; j++) {
                delete[] spData[i][j];
            }
            delete[] spData[i];
        }
        delete[] spData;

        for (int i = 0; i < slicesNo; i++) {
            for (int j = 0; j < imageWidth; j++) {
                delete[] spAnnotationData[i][j];
            }
            delete[] spAnnotationData[i];
        }
        delete[] spAnnotationData;

        for (int i = 0; i < slicesNo; i++) {
            for (int j = 0; j < imageWidth; j++) {
                delete[] manualCorrectionsData[i][j];
            }
            delete[] manualCorrectionsData[i];
        }
        delete[] manualCorrectionsData;

        for (int i = 0; i < slicesNo; i++) {
            for (int j = 0; j < imageWidth; j++) {
                delete[] gridData[i][j];
            }
            delete[] gridData[i];
        }
        delete[] gridData;
    }
    removeComparisonFiles();
}

void AnnotationManager::createActions() {
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *openAct = fileMenu->addAction(tr("&Open"), this, &AnnotationManager::open);
    openAct->setShortcut(QKeySequence::Open);

    saveAct = fileMenu->addAction(tr("&Save annotations"), this, &AnnotationManager::save);
    saveAct->setShortcut(QKeySequence::Save);
    saveAct->setEnabled(false);

    closeImgAct = fileMenu->addAction(tr("&Close image"), this, &AnnotationManager::closeImg);
    closeImgAct->setEnabled(false);

    fileMenu->addSeparator();

    QMenu *segMethodMenu = fileMenu->addMenu(tr("&Segmentation method"));
    segMethodChoiceGroup = new QActionGroup(this);
    segMethodChoiceGroup->setExclusive(true);
    QAction *setLSCAct = segMethodMenu->addAction(tr("LSC"));
    setLSCAct->setData("LSC");
    setLSCAct->setCheckable(true);
    setLSCAct->setChecked(true);
    segMethodChoiceGroup->addAction(setLSCAct);

    QAction *setTPSAct = segMethodMenu->addAction(tr("TPS"));
    setTPSAct->setData("TPS");
    setTPSAct->setCheckable(true);
    segMethodChoiceGroup->addAction(setTPSAct);

    QAction *setManualAct = segMethodMenu->addAction(tr("Manual"));
    setManualAct->setData("MANUAL");
    setManualAct->setCheckable(true);
    segMethodChoiceGroup->addAction(setManualAct);

    connect(segMethodChoiceGroup, SIGNAL(triggered(QAction*)), SLOT(chooseSegmentationMethod(QAction*)));

    QMenu *spNumberMenu = fileMenu->addMenu(tr("&Number of superpixels"));
    spNumberChoiceGroup = new QActionGroup(this);
    spNumberChoiceGroup->setExclusive(true);
    setLessSpAct = spNumberMenu->addAction(tr("Lower (bigger regions)"));
    setLessSpAct->setData("LOWER");
    setLessSpAct->setCheckable(true);
    setLessSpAct->setChecked(true);
    spNumberChoiceGroup->addAction(setLessSpAct);

    setMoreSpAct = spNumberMenu->addAction(tr("Higher (smaller regions)"));
    setMoreSpAct->setData("HIGHER");
    setMoreSpAct->setCheckable(true);
    spNumberChoiceGroup->addAction(setMoreSpAct);

    connect(spNumberChoiceGroup, SIGNAL(triggered(QAction*)), SLOT(chooseSPNumber(QAction*)));

    fileMenu->addSeparator();

    openComparisonImgAct = fileMenu->addAction(tr("&Open additional comparison file"),
                                               this, &AnnotationManager::openComparisonImg);
    openComparisonImgAct->setEnabled(false);

    fileMenu->addSeparator();

    QAction *exitAct = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAct->setShortcut(tr("Ctrl+Q"));

    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

    // TODO Add undo and redo action
    resetAnnotationsAct = editMenu->addAction(tr("&Reset all annotations for current slice"), this, &AnnotationManager::resetAnnotations);
    resetAnnotationsAct->setEnabled(false);

    changeAnnotationsModeAct = editMenu->addAction(tr("&Manual corrections"), this, &AnnotationManager::changeAnnotationMode);
    changeAnnotationsModeAct->setShortcut(Qt::Key_M);
    changeAnnotationsModeAct->setEnabled(false);

    increaseManualPenSizeAct = editMenu->addAction(tr("&Increase manual correction pen size"), this, &AnnotationManager::increaseManualPenSize);
    increaseManualPenSizeAct->setShortcut(tr("Ctrl+Shift++"));
    increaseManualPenSizeAct->setEnabled(false);

    reduceManualPenSizeAct = editMenu->addAction(tr("R&educe manual correction pen size"), this, &AnnotationManager::reduceManualPenSize);
    reduceManualPenSizeAct->setShortcut(tr("Ctrl+Shift+-"));
    reduceManualPenSizeAct->setEnabled(false);

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    zoomInAct = viewMenu->addAction(tr("Zoom &In (25%)"), this, &AnnotationManager::zoomIn);
    zoomInAct->setShortcut(QKeySequence::ZoomIn);
    zoomInAct->setEnabled(false);

    zoomOutAct = viewMenu->addAction(tr("Zoom &Out (25%)"), this, &AnnotationManager::zoomOut);
    zoomOutAct->setShortcut(QKeySequence::ZoomOut);
    zoomOutAct->setEnabled(false);

    normalSizeAct = viewMenu->addAction(tr("&Reset Size"), this, &AnnotationManager::resetSize);
    normalSizeAct->setShortcut(tr("Ctrl+0"));
    normalSizeAct->setEnabled(false);

    viewMenu->addSeparator();

    nextSliceAct = viewMenu->addAction(tr("&Next slice"), this, &AnnotationManager::nextSlice);
    nextSliceAct->setShortcut(Qt::Key_Right);
    nextSliceAct->setEnabled(false);

    previousSliceAct = viewMenu->addAction(tr("&Previous slice"), this, &AnnotationManager::previousSlice);
    previousSliceAct->setShortcut(Qt::Key_Left);
    previousSliceAct->setEnabled(false);

    displayGridAct = viewMenu->addAction(tr("Display &grid"), this, &AnnotationManager::changeDisplayGrid);
    displayGridAct->setShortcut(Qt::Key_G);
    displayGridAct->setEnabled(false);

    displayAnnotationsAct = viewMenu->addAction(tr("Display &annotations"), this, &AnnotationManager::changeDisplayAnnotations);
    displayAnnotationsAct->setShortcut(Qt::Key_A);
    displayAnnotationsAct->setEnabled(false);

    viewMenu->addSeparator();

    nextComparisonImageAct = viewMenu->addAction(tr("Next &comparison image"), this, &AnnotationManager::nextComparisonImage);
    nextComparisonImageAct->setShortcut(Qt::Key_C);
    nextComparisonImageAct->setEnabled(false);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

    helpMenu->addAction(tr("&Instructions"), this, &AnnotationManager::instructions);
}

void AnnotationManager::updateActions() {
    bool filesLoaded = loadedFileName != "";
    saveAct->setEnabled(filesLoaded);
    openComparisonImgAct->setEnabled(filesLoaded);
    closeImgAct->setEnabled(filesLoaded);
    normalSizeAct->setEnabled(filesLoaded);
    resetAnnotationsAct->setEnabled(filesLoaded);
    if (segmentationMethod != "MANUAL") {changeAnnotationsModeAct->setEnabled(filesLoaded);}
    else {changeAnnotationsModeAct->setEnabled(false);}
    increaseManualPenSizeAct->setEnabled(manualCorrectionsMode);
    reduceManualPenSizeAct->setEnabled(manualCorrectionsMode);
    zoomInAct->setEnabled(filesLoaded);
    zoomOutAct->setEnabled(filesLoaded);
    nextSliceAct->setEnabled(filesLoaded);
    previousSliceAct->setEnabled(filesLoaded);
    displayGridAct->setEnabled(filesLoaded);
    displayAnnotationsAct->setEnabled(filesLoaded);
    nextComparisonImageAct->setEnabled(!comparisonData.isEmpty());

    if(filesLoaded) {
        imageType == "SPA" ? setLessSpAct->setText(tr("1000")) : setLessSpAct->setText(tr("1250")); // 1000 for SPA, 1250 for KNEE
        imageType == "SPA" ? setMoreSpAct->setText(tr("2000")) : setMoreSpAct->setText(tr("2500")); // 2000 for SPA, 2500 for KNEE
    } else {
        setLessSpAct->setText(tr("Lower (bigger regions)"));
        setMoreSpAct->setText(tr("Higher (smaller regions)"));
    }
}

void AnnotationManager::mousePressEvent(QMouseEvent *event) {
    if (loadedFileName == ""){return;}
    //position relative to scrollArea beginning, regardless current scrollbars position
    QPoint position =  mapToGlobal(event->pos()) - mapToGlobal(imageLabel->pos()) - splitter->pos();
    position.setX(static_cast<int>(position.x()/scaleFactor));
    position.setY(static_cast<int>(position.y()/scaleFactor));
    if (event->buttons() == Qt::LeftButton) {
        clickedLeft = true;
        if (manualCorrectionsMode) {
            lastManualPoint = position;
            manualCorrectionLine(position, true);
        } else {
            markSuperPixel(position, true);
        }
    } else if (event->buttons() == Qt::RightButton) {
        clickedRight = true;
        if (manualCorrectionsMode) {
            lastManualPoint = position;
            manualCorrectionLine(position, false);
        } else {
            markSuperPixel(position, false);
        }
    }
}

void AnnotationManager::mouseMoveEvent(QMouseEvent *event) {
    if (loadedFileName == ""){return;}
    //position relative to scrollArea beginning, regardless current scrollbars position
    QPoint position =  mapToGlobal(event->pos()) - mapToGlobal(imageLabel->pos()) - splitter->pos();
    position.setX(static_cast<int>(position.x()/scaleFactor));
    position.setY(static_cast<int>(position.y()/scaleFactor));
    if ((event->buttons() & Qt::LeftButton) && clickedLeft) {
        if (manualCorrectionsMode) {
            manualCorrectionLine(position, true);
        } else {
            markSuperPixel(position, true);
        }
    } else if ((event->buttons() & Qt::RightButton) && clickedRight) {
        if (manualCorrectionsMode) {
            manualCorrectionLine(position, false);
        } else {
            markSuperPixel(position, false);
        }
    }
}

void AnnotationManager::mouseReleaseEvent(QMouseEvent *event) {
    if (loadedFileName == ""){return;}
    //position relative to scrollArea beginning, regardless current scrollbars position
    QPoint position =  mapToGlobal(event->pos()) - mapToGlobal(imageLabel->pos()) - splitter->pos();
    position.setX(static_cast<int>(position.x()/scaleFactor));
    position.setY(static_cast<int>(position.y()/scaleFactor));
    if ((event->buttons() == Qt::LeftButton) && clickedLeft) {
        clickedLeft = false;
        if (manualCorrectionsMode) {
            manualCorrectionLine(position, true);
        } else {
            markSuperPixel(position, true);
        }
    } else if ((event->buttons() == Qt::RightButton) && clickedRight) {
        clickedRight = false;
        if (manualCorrectionsMode) {
            manualCorrectionLine(position, false);
        } else {
            markSuperPixel(position, false);
        }
    }
}

void AnnotationManager::manualCorrectionLine(QPoint &endPoint, const bool &adding) { //Bresenham's line algorithm
    int x0 = endPoint.x();
    int y0 = endPoint.y();
    int x1 = lastManualPoint.x();
    int y1 = lastManualPoint.y();

    int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1;
    int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1;
    int err = dx+dy, err2; // error value e_xy, 2*e_xy

    while (true) {
        // Iterating over pixels inside circle with radius manualPenSize and center in (x0,y0)
        for (int yw = -manualPenSize; yw <= manualPenSize; yw++) {
            int dxw = static_cast<int> (sqrt(manualPenSize * manualPenSize - yw * yw));
            for (int xw = -dxw; xw <= dxw; xw++)
                markPixel(x0+xw, y0+yw, adding);
        }
        updateDisplay();
        if (x0==x1 && y0==y1) break;
        err2 = 2 * err;
        if (err2 >= dy) { err += dy; x0 += sx; } // e_xy+e_x > 0
        if (err2 <= dx) { err += dx; y0 += sy; } // e_xy+e_y < 0
    }

    lastManualPoint = endPoint;
    unsavedChanges = true;
}

void AnnotationManager::markPixel(const int &x, const int &y, const bool &adding) {
    if (x<0 || x>imageWidth-1 || y<0 || y>imageHeight-1) {return;}

    if (adding) { // Adding new pixels to annotation
        if (!spAnnotationData[currSlice][x][y]) {
            manualCorrectionsData[currSlice][x][y] = 1;
        } else {
            manualCorrectionsData[currSlice][x][y] = 0;
        }
    } else { // Removing pixels from annotation
        if (spAnnotationData[currSlice][x][y]) {
            manualCorrectionsData[currSlice][x][y] = -1;
        } else {
            manualCorrectionsData[currSlice][x][y] = 0;
        }
    }
}

void AnnotationManager::markSuperPixel(const QPoint &position, const bool &adding) {
    int x = position.x();
    int y = position.y();

    if (x < 0 || x > imageWidth - 1 || y < 0 || y > imageHeight - 1) { return; }

    unsigned short chosenColor = spData[currSlice][x][y];

    std::queue<std::pair<int, int>> pointsQueue;
    pointsQueue.emplace(x, y);

    std::pair<int, int> currPoint;
    int currX;
    int currY;

    while (!pointsQueue.empty()) {
        currPoint = pointsQueue.front();
        pointsQueue.pop();
        currX = currPoint.first;
        currY = currPoint.second;

        // Going through chosen point and it's neighbours (if it's inside image area)
        for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++) {
                if (currX + i >= 0 && currX + i < imageWidth && currY + j >= 0 && currY + j < imageHeight) {
                    if (spData[currSlice][currX + i][currY + j] == chosenColor) {
                        if (adding) { // Adding new super pixels to annotation
                            if (spAnnotationData[currSlice][currX + i][currY + j] == 0) {
                                pointsQueue.emplace(currX + i, currY + j);
                                spAnnotationData[currSlice][currX + i][currY + j] = 1;
                                if (manualCorrectionsData[currSlice][currX + i][currY + j] == 1) {
                                    manualCorrectionsData[currSlice][currX + i][currY + j] = 0;
                                    // If there was correction in area of newly added superpixel it's removed
                                }
                            }
                        } else { // Removing superpixels from annotation
                            if (spAnnotationData[currSlice][currX + i][currY + j]) {
                                pointsQueue.emplace(currX + i, currY + j);
                                spAnnotationData[currSlice][currX + i][currY + j] = 0;
                                if (manualCorrectionsData[currSlice][currX + i][currY + j] == -1) {
                                    manualCorrectionsData[currSlice][currX + i][currY + j] = 0;
                                    // If there was correction in area of newly removed superpixel it's removed
                                }
                            }
                        }
                    }
                }
            }
    }
    updateDisplay();
    unsavedChanges = true;
}

bool AnnotationManager::loadFiles(const QString &fileName){
    QDir fileDir(fileName);
    QFileInfo fileInfo(fileName);
    QStringList imgParams=fileInfo.fileName().split("_");

    imageType = imgParams[imgParams.size()-7];
    int patientNo = imgParams[imgParams.size()-6].toInt();
    imageWidth = imgParams[imgParams.size()-5].toInt();
    imageHeight = imgParams[imgParams.size()-4].toInt();
    slicesNo = imgParams[imgParams.size()-3].toInt();

    stirData = new unsigned short **[slicesNo];
    for (int i = 0; i < slicesNo; i++) {
        stirData[i] = new unsigned short *[imageWidth];

        for (int j = 0; j < imageWidth; j++)
            stirData[i][j] = new unsigned short [imageHeight];
    }

    spData = new unsigned short **[slicesNo];
    for (int i = 0; i < slicesNo; i++) {
        spData[i] = new unsigned short *[imageWidth];

        for (int j = 0; j < imageWidth; j++)
            spData[i][j] = new unsigned short [imageHeight];
    }

    spAnnotationData = new char **[slicesNo];
    for (int i = 0; i < slicesNo; i++) {
        spAnnotationData[i] = new char *[imageWidth];

        for (int j = 0; j < imageWidth; j++)
            spAnnotationData[i][j] = new char [imageHeight];
    }

    manualCorrectionsData = new char **[slicesNo];
    for (int i = 0; i < slicesNo; i++) {
        manualCorrectionsData[i] = new char *[imageWidth];

        for (int j = 0; j < imageWidth; j++)
            manualCorrectionsData[i][j] = new char [imageHeight];
    }

    gridData = new bool **[slicesNo];
    for (int i = 0; i < slicesNo; i++) {
        gridData[i] = new bool *[imageWidth];

        for (int j = 0; j < imageWidth; j++)
            gridData[i][j] = new bool [imageHeight];
    }

    loadRaw(fileName, stirData);
    rescaleData(stirData);

    if (segmentationMethod != "MANUAL") {
        QString spNumberVal;

        if (spNumber == "LOWER") {
            spNumberVal = imageType == "SPA" ? "1000" : "1250"; // 1000 for SPA, 1250 for KNEE
        } else {
            spNumberVal = imageType == "SPA" ? "2000" : "2500"; // 2000 for SPA, 2500 for KNEE
        }

        if (!fileDir.cd("../../segmentations/superpixels/" + imageType + spNumberVal + segmentationMethod)) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Cannot find %0 segmentation data for %1 with %2 superpixels!")
                                             .arg(segmentationMethod).arg(imageType).arg(spNumberVal));
            return false;
        }

        if (!loadRaw(fileDir.path() + QString(QDir::separator()) + QString("%0SuperPixel%1_%2_%3_%4_%5_2_.raw")
                             .arg(spNumberVal).arg(segmentationMethod).arg(patientNo).arg(imageWidth).arg(imageHeight).arg(slicesNo),
                     spData)) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Cannot find segmentation data! "
                                        "Please make sure it is available and load the file again."));
            return false;
        }

        if (!fileDir.cd("../../grids/" + imageType + spNumberVal + segmentationMethod)) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Cannot find %0 grid data for %1 with %2 superpixels!")
                                             .arg(segmentationMethod).arg(imageType).arg(spNumberVal));
            return false;
        }

        if (!loadRaw(fileDir.path() + QString(QDir::separator()) + QString("%0BorderSuperPixel%1_%2_%3_%4_%5_2_.raw")
                             .arg(spNumberVal).arg(segmentationMethod).arg(patientNo).arg(imageWidth).arg(imageHeight).arg(slicesNo),
                     gridData)) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Cannot find grid data! "
                                        "Please make sure it is available and load the file again."));
            return false;
        }

        if (!fileDir.mkpath("../../../annotations/sp/" + imageType + spNumberVal + segmentationMethod)) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Could not create annotations directory!"));
            return false;
        }

        if (!fileDir.mkpath("../../../annotations/manual/" + imageType + spNumberVal + segmentationMethod)) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Could not create annotations directory!"));
            return false;
        }

        fileDir.cd("../../../annotations/sp/" + imageType + spNumberVal + segmentationMethod);

        spAnnFileName = fileDir.path() + QString(QDir::separator()) + QString("%0spAnnotations%1_%2_%3_%4_%5_1_.raw")
                .arg(spNumberVal).arg(segmentationMethod).arg(patientNo).arg(imageWidth).arg(imageHeight).arg(slicesNo);

        if (QFileInfo::exists(spAnnFileName)) {
            loadRaw(spAnnFileName, spAnnotationData);
        } else {
            for (int sl_no = 0; sl_no < slicesNo; sl_no++)
                for (int y = 0; y < imageHeight; y++)
                    for (int x = 0; x < imageWidth; x++) {
                        spAnnotationData[sl_no][x][y] = 0;
                    }
        }

        fileDir.cd("../../manual/" + imageType + spNumberVal + segmentationMethod);


        manualCorrFileName =
                fileDir.path() + QString(QDir::separator()) + QString("%0manualAnnotations%1_%2_%3_%4_%5_1_.raw")
                        .arg(spNumberVal).arg(segmentationMethod).arg(patientNo).arg(imageWidth).arg(imageHeight).arg(
                        slicesNo);

        if (QFileInfo::exists(manualCorrFileName)) {
            loadRaw(manualCorrFileName, manualCorrectionsData);
        } else {
            for (int sl_no = 0; sl_no < slicesNo; sl_no++)
                for (int y = 0; y < imageHeight; y++)
                    for (int x = 0; x < imageWidth; x++) {
                        manualCorrectionsData[sl_no][x][y] = 0;
                    }
        }
    } else {
        fileDir.cd("../../");
        if (!fileDir.mkpath("annotations/manual/" + imageType + "MANUAL")) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Could not create annotations directory!"));
            return false;
        }
        fileDir.cd("annotations/manual/" + imageType + "MANUAL");

        manualCorrFileName =
                fileDir.path() + QString(QDir::separator()) + QString("0manualAnnotationsMANUAL%1_%2_%3_%4_1_.raw")
                        .arg(patientNo).arg(imageWidth).arg(imageHeight).arg(slicesNo);

        if (QFileInfo::exists(manualCorrFileName)) {
            loadRaw(manualCorrFileName, manualCorrectionsData);
        } else {
            for (int sl_no = 0; sl_no < slicesNo; sl_no++)
                for (int y = 0; y < imageHeight; y++)
                    for (int x = 0; x < imageWidth; x++) {
                        manualCorrectionsData[sl_no][x][y] = 0;
                    }
        }

        for (int sl_no = 0; sl_no < slicesNo; sl_no++)
            for (int y = 0; y < imageHeight; y++)
                for (int x = 0; x < imageWidth; x++) {
                    spAnnotationData[sl_no][x][y] = 0;
                }

        manualCorrectionsMode = true;
    }

    currSlice = 0;
    scaleFactor = 1.0;
    loadedFileName = fileName;
    setWindowFilePath(fileName);
    updateActions();
    updateDisplay();

    return true;
}

bool AnnotationManager::loadRaw(const QString &fileName, unsigned short ***dataArray) const {
    std::ifstream imageFileStream;
    imageFileStream.open(fileName.toStdString(), std::ios::ate | std::ios::binary);
    if (imageFileStream.fail()){
        imageFileStream.close();
        return false;
    } else {
        std::streampos size;
        char *memBlock;
        size = imageFileStream.tellg();
        memBlock = new char[size];

        imageFileStream.seekg(0, std::ios::beg);
        imageFileStream.read(memBlock, size);

        int currByte = 0;
        for (int sl_no = 0; sl_no < slicesNo; sl_no++)
            for (int y = 0; y < imageHeight; y++)
                for (int x = 0; x < imageWidth; x++) {
                    dataArray[sl_no][x][y] = 256 * static_cast<unsigned char>(memBlock[currByte])
                                             + static_cast<unsigned char>(memBlock[currByte + 1]);
                    currByte += 2;
                }
        delete[] memBlock;
        imageFileStream.close();
    }

    return true;
}

bool AnnotationManager::loadRaw(const QString &fileName, char ***dataArray) const {
    std::ifstream imageFileStream;
    imageFileStream.open(fileName.toStdString(), std::ios::ate | std::ios::binary);
    if (imageFileStream.fail()){
        imageFileStream.close();
        return false;
    } else {
        std::streampos size;
        char *memBlock;
        size = imageFileStream.tellg();
        memBlock = new char[size];

        imageFileStream.seekg(0, std::ios::beg);
        imageFileStream.read(memBlock, size);

        for (int sl_no = 0; sl_no < slicesNo; sl_no++)
            for (int y = 0; y < imageHeight; y++)
                for (int x = 0; x < imageWidth; x++) {
                    dataArray[sl_no][x][y] = memBlock[sl_no * imageHeight * imageWidth + y * imageWidth + x];
                }
        delete[] memBlock;
        imageFileStream.close();
    }

    return true;
}

bool AnnotationManager::loadRaw(const QString &fileName, bool ***dataArray) const {
    std::ifstream imageFileStream;
    imageFileStream.open(fileName.toStdString(), std::ios::ate | std::ios::binary);
    if (imageFileStream.fail()){
        imageFileStream.close();
        return false;
    } else {
        std::streampos size;
        char *memBlock;
        size = imageFileStream.tellg();
        memBlock = new char[size];

        imageFileStream.seekg(0, std::ios::beg);
        imageFileStream.read(memBlock, size);

        int currByte = 0;

        for (int sl_no = 0; sl_no < slicesNo; sl_no++)
            for (int y = 0; y < imageHeight; y++)
                for (int x = 0; x < imageWidth; x++) {
                    dataArray[sl_no][x][y] = 256 * static_cast<unsigned char>(memBlock[currByte])
                                             + static_cast<unsigned char>(memBlock[currByte + 1]) > 0;
                    currByte += 2;
                }
        delete[] memBlock;
        imageFileStream.close();
    }
    return true;
}

bool AnnotationManager::loadComparisonFile(const QString &fileName) {
    auto ***currImageData = new unsigned short **[slicesNo];
    for (int i = 0; i < slicesNo; i++) {
        currImageData[i] = new unsigned short *[imageWidth];

        for (int j = 0; j < imageWidth; j++)
            currImageData[i][j] = new unsigned short [imageHeight];
    }

    if (!loadRaw(fileName, currImageData)) {return false;}

    rescaleData(currImageData);

    comparisonData.append(currImageData);

    comparisonFileNo = comparisonData.size()-1;

    updateActions();
    updateDisplay();

    return true;
}

bool AnnotationManager::saveRaw(const QString &fileName, char ***dataArray) const {
    std::ofstream imageFileStream;
    imageFileStream.open(fileName.toStdString(), std::ios::binary);
    if (imageFileStream.fail()){
        imageFileStream.close();
        return false;
    } else {
        for (int sl_no = 0; sl_no < slicesNo; sl_no++)
            for (int y = 0; y < imageHeight; y++)
                for (int x = 0; x < imageWidth; x++) {
                    imageFileStream << dataArray[sl_no][x][y];
                }
        imageFileStream.close();
    }
    return true;
}

bool AnnotationManager::rescaleData(unsigned short ***dataArray) const {
    unsigned short maxVal {1};
    for (int sl_no = 0; sl_no < slicesNo; sl_no++)
        for (int x = 0; x < imageWidth; x++)
            for (int y = 0; y < imageHeight; y++)
                if (maxVal < dataArray[sl_no][x][y]){ maxVal=dataArray[sl_no][x][y];}

    for (int sl_no = 0; sl_no < slicesNo; sl_no++)
        for (int x = 0; x < imageWidth; x++)
            for (int y = 0; y < imageHeight; y++)
                dataArray[sl_no][x][y] *= 65535 / maxVal;

    return true;
}

void AnnotationManager::updateDisplay() {
    QPixmap display(imageWidth, imageHeight);
    QPainter painter(&display);

    QImage stirImage (imageWidth, imageHeight, QImage::Format_RGBA64);
    QImage gridImage (imageWidth, imageHeight, QImage::Format_RGBA64);
    QImage annotationImage (imageWidth, imageHeight, QImage::Format_RGBA64);

    QRgba64 colorValue = {};

    for (int x = 0; x < imageWidth; x++)
        for (int y = 0; y < imageHeight; y++) {
            colorValue = qRgba64(stirData[currSlice][x][y], stirData[currSlice][x][y], stirData[currSlice][x][y], 65535);
            stirImage.setPixelColor(x, y, colorValue);
        }
    painter.drawImage(QPoint(0,0), stirImage);

    if(displayAnnotations) {
        for (int x = 0; x < imageWidth; x++)
            for (int y = 0; y < imageHeight; y++) {
                if (spAnnotationData[currSlice][x][y] + manualCorrectionsData[currSlice][x][y] > 0) {
                    colorValue = qRgba64(65535, 0, 0, 32767);
                } else {
                    colorValue = qRgba64(65535, 0, 0, 0);
                }
                annotationImage.setPixelColor(x, y, colorValue);
            }
        painter.drawImage(QPoint(0,0), annotationImage);
    }

    if(displayGrid) {
        for (int x = 0; x < imageWidth; x++)
            for (int y = 0; y < imageHeight; y++) {
                if (gridData[currSlice][x][y]) {
                    colorValue = qRgba64(0, 65535, 0, 65535);
                } else {
                    colorValue = qRgba64(0, 65535, 0, 0);
                }
                gridImage.setPixelColor(x, y, colorValue);
            }
        painter.drawImage(QPoint(0,0), gridImage);
    }

    painter.end();

    int horizontalScrollValue = scrollArea->horizontalScrollBar()->value();
    int verticalScrollValue = scrollArea->verticalScrollBar()->value();

    imageLabel->setPixmap(display);
    scrollArea->setVisible(true);
    imageLabel->adjustSize();
    scrollArea->horizontalScrollBar()->setValue(horizontalScrollValue);
    scrollArea->verticalScrollBar()->setValue(verticalScrollValue);

    if(comparisonFileNo > -1) {
        QPixmap comparisonDisplay(imageWidth, imageHeight);
        QPainter comparisonPainter(&comparisonDisplay);
        QImage comparisonImage (imageWidth, imageHeight, QImage::Format_RGBA64);

        for (int x = 0; x < imageWidth; x++)
            for (int y = 0; y < imageHeight; y++) {
                unsigned short val = comparisonData[comparisonFileNo][currSlice][x][y];
                colorValue = qRgba64(val, val, val, 65535);
                comparisonImage.setPixelColor(x, y, colorValue);
            }

        comparisonPainter.drawImage(QPoint(0,0), comparisonImage);
        comparisonPainter.end();

        comparisonImageLabel->setPixmap(comparisonDisplay);
        comparisonScrollArea->setVisible(true);
        comparisonImageLabel->adjustSize();
        comparisonScrollArea->horizontalScrollBar()->setValue(horizontalScrollValue);
        comparisonScrollArea->verticalScrollBar()->setValue(verticalScrollValue);
    } else {;
        comparisonScrollArea->setVisible(false);
    }

    scaleImages(1);
}

void AnnotationManager::open() {
    QFileDialog dialog(this, tr("Open image"));
    static bool firstDialog = true;
    static QDir lastFileDir;

    if (firstDialog) {
        firstDialog = false;
        const QStringList picturesLocations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
        dialog.setDirectory(picturesLocations.isEmpty() ? QDir::currentPath() : picturesLocations.last());
    } else {
        dialog.setDirectory(lastFileDir);
    }

    dialog.setNameFilter("RAW files (*.raw)");

    bool loaded = false;

    while (dialog.exec() == QDialog::Accepted) {
        loaded = loadFiles(dialog.selectedFiles().first());
        if(loaded) {break;}
    }

    if (loaded) {
        lastFileDir = QFileInfo(dialog.selectedFiles().first()).absoluteDir();
        loadedFileSize = QFileInfo(dialog.selectedFiles().first()).size();
    }
}

void AnnotationManager::openComparisonImg() {
    QFileDialog dialog(this, tr("Open additional image for comparison"));
    static bool firstDialog = true;
    static QDir lastFileDir;

    if (firstDialog) {
        firstDialog = false;
        const QStringList picturesLocations = QStandardPaths::standardLocations(QStandardPaths::PicturesLocation);
        dialog.setDirectory(picturesLocations.isEmpty() ? QDir::currentPath() : picturesLocations.last());
    } else {
        dialog.setDirectory(lastFileDir);
    }

    dialog.setNameFilter("RAW files (*.raw)");

    bool loaded = false;

    while (dialog.exec() == QDialog::Accepted) {
        if(QFileInfo(dialog.selectedFiles().first()).size() == loadedFileSize) {
            loaded = loadComparisonFile(dialog.selectedFiles().first());
        } else {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Chosen comparison image file size does not match the size of main image! "
                                        "Please try again."));
        }
        if(loaded) {break;}
    }

    if (loaded) {
        lastFileDir = QFileInfo(dialog.selectedFiles().first()).absoluteDir();
    }
}

void AnnotationManager::save() {
    if (segmentationMethod != "MANUAL") {
        if (!saveRaw(spAnnFileName, spAnnotationData)) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Could not save annotations, please try again."));
            return;
        }
    }
    if (!saveRaw(manualCorrFileName, manualCorrectionsData)) {
        QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                 tr("Could not save annotations, please try again."));
        return;
    }

    statusBar()->showMessage(tr("Annotations have been saved!"));
    unsavedChanges = false;
}

void AnnotationManager::closeImg() {
    if (unsavedChanges) {
        QMessageBox::StandardButton answer =
                QMessageBox::question(this, "Unsaved changes", "Save annotations before leaving?",
                                      QMessageBox::Discard | QMessageBox::Cancel | QMessageBox::Save);
        if (answer == QMessageBox::Cancel) {
            return;
        } else if (answer == QMessageBox::Save) {
            save();
        }
    }
    setWindowFilePath("");
    imageLabel->setPixmap(QPixmap());
    scrollArea->setVisible(false);

    loadedFileName = "";
    loadedFileSize = 0;
    unsavedChanges = false;
    updateActions();

    removeComparisonFiles();
    comparisonImageLabel->setPixmap(QPixmap());
    comparisonScrollArea->setVisible(false);
}

void AnnotationManager::removeComparisonFiles() {
    for (auto*** currImageData : comparisonData) {
        for (int i = 0; i < slicesNo; i++) {
            for (int j = 0; j < imageWidth; j++) {
                delete[] currImageData[i][j];
            }
            delete[] currImageData[i];
        }
        delete[] currImageData;
    }

    while(!comparisonData.isEmpty()) {
        comparisonData.removeLast();
    }

    comparisonFileNo = -1;
}

void AnnotationManager::chooseSegmentationMethod(QAction* chooseMethodAct) {
    if (segmentationMethod != chooseMethodAct->data().toString()){
        segmentationMethod = chooseMethodAct->data().toString();
        if (loadedFileName != "") {
            QString fileNameToRemember = loadedFileName;
            closeImg();
            loadFiles(fileNameToRemember);
        }
    }
}

void AnnotationManager::chooseSPNumber(QAction *chooseSPNumberAct) {
    if (spNumber != chooseSPNumberAct->data().toString()){
        spNumber = chooseSPNumberAct->data().toString();
        if (loadedFileName != "") {
            QString fileNameToRemember = loadedFileName;
            closeImg();
            loadFiles(fileNameToRemember);
        }
    }
}

void AnnotationManager::closeEvent(QCloseEvent *event)
{
    if (unsavedChanges) {
        event->ignore();
        QMessageBox::StandardButton answer =
                QMessageBox::question(this, "Unsaved changes","Save annotations before leaving?",
                                      QMessageBox::Discard | QMessageBox::Cancel | QMessageBox::Save);
        if (answer == QMessageBox::Save) {
            save();
            event->accept();
        } else if (answer == QMessageBox::Discard) {
            event->accept();
        }
    }
}

void AnnotationManager::resetAnnotations() {
    for (int x = 0; x < imageWidth; x++)
        for (int y = 0; y < imageHeight; y++) {
            spAnnotationData[currSlice][x][y] = 0;
            manualCorrectionsData[currSlice][x][y] = 0;
        }
    updateDisplay();
}

void AnnotationManager::changeAnnotationMode() {
    manualCorrectionsMode = !manualCorrectionsMode;
    updateActions();
}

void AnnotationManager::increaseManualPenSize() {
    if (manualPenSize < 10) {manualPenSize++;}
}

void AnnotationManager::reduceManualPenSize() {
    if (manualPenSize > 0) {manualPenSize--;}
}

void AnnotationManager::zoomIn()
{
    scaleImages(1.25);
}

void AnnotationManager::zoomOut()
{
    scaleImages(0.8);
}

void AnnotationManager::resetSize()
{
    imageLabel->adjustSize();
    comparisonImageLabel->adjustSize();
    scaleFactor = 1.0;
}

void AnnotationManager::scaleImages(double factor)
{
    scaleFactor *= factor;
    imageLabel->resize(scaleFactor * imageLabel->pixmap(Qt::ReturnByValue).size());
    comparisonImageLabel->resize(scaleFactor * comparisonImageLabel->pixmap(Qt::ReturnByValue).size());

    adjustScrollBar(scrollArea->horizontalScrollBar(), factor);
    adjustScrollBar(scrollArea->verticalScrollBar(), factor);

    zoomInAct->setEnabled(scaleFactor < 5.0);
    zoomOutAct->setEnabled(scaleFactor > 0.2);
}

void AnnotationManager::adjustScrollBar(QScrollBar *scrollBar, double factor)
{
    scrollBar->setValue(int(factor * scrollBar->value() + ((factor - 1) * scrollBar->pageStep()/2)));
}

void AnnotationManager::nextSlice() {
    if (currSlice<slicesNo-1) {currSlice++;}
    updateDisplay();
}

void AnnotationManager::previousSlice() {
    if (currSlice>0) {currSlice--;}
    updateDisplay();
}

void AnnotationManager::changeDisplayGrid() {
    displayGrid = !displayGrid;
    updateDisplay();
}

void AnnotationManager::changeDisplayAnnotations() {
    displayAnnotations = !displayAnnotations;
    updateDisplay();
}

void AnnotationManager::nextComparisonImage() {
    if(comparisonData.size() > 1) {
        comparisonFileNo = (comparisonFileNo + 1) % comparisonData.size();
        updateDisplay();
    }
}

void AnnotationManager::instructions() {
    QMessageBox::about(this, tr("Instructions"),
                       tr("<p><b>Instructions:</b></p>"
                          "<p>1. Load RAW file containing STIR images to start working with application. "
                          "Please do not change file names nor directory structure.</p>"
                          "<p>2. Segmentation files are loaded automatically. "
                          "Choose segmentation method for which you want to make annotations in File menu.</p>"
                          "<p>3. Use left mouse button to mark new region as a lesion.</p>"
                          "<p>4. Use right mouse button to remove part of annotation.</p>"
                          "<p>5. To make corrections turn on manual corrections mode (M key). "
                          "This mode is permanently turned on if Manual segmentation method is chosen "
                          "It allows to make annotations regardless image segmentation. "
                          "You can adjust size of correction pen in Edit menu (or using Ctrl+Shift++/Ctrl+Shift+-).</p>"
                          "<p>6. Remember to save annotations once you finish your work in File menu (Ctrl+S)."
                          "Annotations for all slices are saved at once. </p>"
                          "<p>7. You can load any number of additional images for comparison in File menu. "
                          "To switch to next image press C or choose proper option in View menu. </p>"
                          ));
}
