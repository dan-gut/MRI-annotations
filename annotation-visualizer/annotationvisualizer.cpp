#include "annotationvisualizer.h"

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

#include <iostream>
#include <fstream>
#include <queue>


AnnotationVisualizer::AnnotationVisualizer(QWidget *parent)
        : QMainWindow(parent), imageLabel(new QLabel)
        , scrollArea(new QScrollArea)
{
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents(true);

    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(imageLabel);
    scrollArea->setVisible(false);
    setCentralWidget(scrollArea);

    createActions();

    resize(QGuiApplication::primaryScreen()->availableSize() * 3 / 5);
}

AnnotationVisualizer::~AnnotationVisualizer() {
    if (imageHeight != 0 && imageWidth != 0 && slicesNo != 0) {
        for (int i = 0; i < slicesNo; i++) {
            for (int j = 0; j < imageWidth; j++) {
                delete[] stirData[i][j];
            }
            delete[] stirData[i];
        }
        delete[] stirData;
    }

    for (int i = 0; i < slicesNo; i++) {
        for (int j = 0; j < imageWidth; j++) {
            delete[] gridData[i][j];
        }
        delete[] gridData[i];
    }
    delete[] gridData;

    for (int i = 0; i < annotatorsList.size(); i++) {
        for (int j = 0; j < slicesNo; j++) {
            for (int k = 0; k < imageWidth; k++) {
                delete[] spAnnotationData[i][j][k];
            }
            delete[] spAnnotationData[i][j];
        }
        delete[] spAnnotationData[i];
    }
    delete[] spAnnotationData;

    for (int i = 0; i < annotatorsList.size(); i++) {
        for (int j = 0; j < slicesNo; j++) {
            for (int k = 0; k < imageWidth; k++) {
                delete[] manualCorrectionsData[i][j][k];
            }
            delete[] manualCorrectionsData[i][j];
        }
        delete[] manualCorrectionsData[i];
    }
    delete[] manualCorrectionsData;
}

void AnnotationVisualizer::createActions() {
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *openAct = fileMenu->addAction(tr("&Open"), this, &AnnotationVisualizer::open);
    openAct->setShortcut(QKeySequence::Open);

    closeImgAct = fileMenu->addAction(tr("&Close image"), this, &AnnotationVisualizer::closeImg);
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

    QAction *exitAct = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAct->setShortcut(tr("Ctrl+Q"));

    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    zoomInAct = viewMenu->addAction(tr("Zoom &In (25%)"), this, &AnnotationVisualizer::zoomIn);
    zoomInAct->setShortcut(QKeySequence::ZoomIn);
    zoomInAct->setEnabled(false);

    zoomOutAct = viewMenu->addAction(tr("Zoom &Out (25%)"), this, &AnnotationVisualizer::zoomOut);
    zoomOutAct->setShortcut(QKeySequence::ZoomOut);
    zoomOutAct->setEnabled(false);

    normalSizeAct = viewMenu->addAction(tr("&Reset Size"), this, &AnnotationVisualizer::resetSize);
    normalSizeAct->setShortcut(tr("Ctrl+0"));
    normalSizeAct->setEnabled(false);

    viewMenu->addSeparator();

    nextSliceAct = viewMenu->addAction(tr("&Next slice"), this, &AnnotationVisualizer::nextSlice);
    nextSliceAct->setShortcut(Qt::Key_Right);
    nextSliceAct->setEnabled(false);

    previousSliceAct = viewMenu->addAction(tr("&Previous slice"), this, &AnnotationVisualizer::previousSlice);
    previousSliceAct->setShortcut(Qt::Key_Left);
    previousSliceAct->setEnabled(false);

    displayGridAct = viewMenu->addAction(tr("Display &grid"), this, &AnnotationVisualizer::changeDisplayGrid);
    displayGridAct->setShortcut(Qt::Key_G);
    displayGridAct->setEnabled(false);

    annotationsMenu = menuBar()->addMenu(tr("&Annotations"));

    hideAnnotationsAct = annotationsMenu->addAction(tr("Hide annotations"), this, &AnnotationVisualizer::hideAnnotations);
    hideAnnotationsAct->setShortcut(Qt::Key_A);
    hideAnnotationsAct->setEnabled(false);

    annotationsMenu->addSection(tr("Displayed annotations"));

    annotationsDisplayChoiceGroup = new QActionGroup(this);
    annotationsDisplayChoiceGroup->setExclusive(true);

    QAction *setManualAct = annotationsMenu->addAction(tr("Manual corrections only"));
    setManualAct->setData("MANUAL");
    setManualAct->setCheckable(true);
    annotationsDisplayChoiceGroup->addAction(setManualAct);

    QAction *setSPAct = annotationsMenu->addAction(tr("SP annotations only"));
    setSPAct->setData("SP");
    setSPAct->setCheckable(true);
    annotationsDisplayChoiceGroup->addAction(setSPAct);

    QAction *setBothAct = annotationsMenu->addAction(tr("Complete annotations"));
    setBothAct->setData("BOTH");
    setBothAct->setCheckable(true);
    setBothAct->setChecked(true);
    annotationsDisplayChoiceGroup->addAction(setBothAct);

    connect(annotationsDisplayChoiceGroup, SIGNAL(triggered(QAction *)), SLOT(changeDisplayedAnnotations(QAction *)));

    annotatorsChoiceGroup = new QActionGroup(this);
    annotatorsChoiceGroup->setExclusive(false);
    annotationsMenu->addSection(tr("Displayed annotators"));

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

    helpMenu->addAction(tr("&Instructions"), this, &AnnotationVisualizer::instructions);
}

void AnnotationVisualizer::addAnnotatorsActions() {
    while (!annotatorsChoiceGroup->actions().empty()){
        annotationsMenu->removeAction(annotatorsChoiceGroup->actions().at(0));
        annotatorsChoiceGroup->removeAction(annotatorsChoiceGroup->actions().at(0));
    }

    for (int i = 0; i < annotatorsList.size(); i++) {
        QAction *nextAnnotatorAct = annotationsMenu->addAction(annotatorsList.at(i), this,
                                                               &AnnotationVisualizer::updateDisplay);
        nextAnnotatorAct->setCheckable(true);
        nextAnnotatorAct->setChecked(true);
        annotatorsChoiceGroup->addAction(nextAnnotatorAct);
    }
}

void AnnotationVisualizer::updateActions() {
    bool filesLoaded = loadedFileName != "";
    closeImgAct->setEnabled(filesLoaded);
    normalSizeAct->setEnabled(filesLoaded);
    zoomInAct->setEnabled(filesLoaded);
    zoomOutAct->setEnabled(filesLoaded);
    nextSliceAct->setEnabled(filesLoaded);
    previousSliceAct->setEnabled(filesLoaded);
    displayGridAct->setEnabled(filesLoaded && gridDataAvailable);
    hideAnnotationsAct->setEnabled(filesLoaded);

    imageType == "SPA" ? setLessSpAct->setText(tr("1000")) : setLessSpAct->setText(tr("250"));
    imageType == "SPA" ? setMoreSpAct->setText(tr("2000")) : setMoreSpAct->setText(tr("500"));
}

bool AnnotationVisualizer::loadFiles(const QString &fileName){
    QDir fileDir(fileName);
    QFileInfo fileInfo(fileName);
    QStringList imgParams=fileInfo.fileName().split("_");

    imageType = imgParams[imgParams.size()-7];
    patientNo = imgParams[imgParams.size()-6].toInt();
    imageWidth = imgParams[imgParams.size()-5].toInt();
    imageHeight = imgParams[imgParams.size()-4].toInt();
    slicesNo = imgParams[imgParams.size()-3].toInt();

    stirData = new unsigned short **[slicesNo];
    for (int i = 0; i < slicesNo; i++) {
        stirData[i] = new unsigned short *[imageWidth];

        for (int j = 0; j < imageWidth; j++)
            stirData[i][j] = new unsigned short [imageHeight];
    }

    gridData = new bool **[slicesNo];
    for (int i = 0; i < slicesNo; i++) {
        gridData[i] = new bool *[imageWidth];

        for (int j = 0; j < imageWidth; j++)
            gridData[i][j] = new bool [imageHeight];
    }

    loadRaw(fileName, stirData);

    QString spNumberVal;

    if (spNumber == "LOWER") {
        spNumberVal = imageType == "SPA" ? "1000" : "250";
    } else {
        spNumberVal = imageType == "SPA" ? "2000" : "500";
    }

    if (!fileDir.cd("../../segmentations/grids/"  + imageType + spNumberVal + segmentationMethod)){
        QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                 tr("%0 grid data for %1 with %2 superpixels is not available. "
                                    "Grid cannot be displayed, please provide proper file "
                                    "and reload the image to activate this feature")
                                    .arg(imageType).arg(segmentationMethod).arg(spNumberVal));
        gridDataAvailable = false;
    } else {
        if (!loadRaw(fileDir.path() + QString(QDir::separator()) +
        QString("%0BorderSuperPixel%1_%2_%3_%4_%5_2_.raw").arg(spNumberVal).arg(segmentationMethod)
        .arg(patientNo).arg(imageWidth).arg(imageHeight).arg(slicesNo), gridData)) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Cannot find grid data! "
                                        "Please make sure it is available and load the file again."));
            gridDataAvailable = false;
        } else {
            gridDataAvailable = true;
        }
    }

    fileDir.cd("../../../annotations/");

    annotatorsList = fileDir.entryList(QDir::Dirs);
    annotatorsList.removeOne("..");
    annotatorsList.removeOne(".");

    addAnnotatorsActions();
    loadAnnotations(fileDir);

    currSlice = 0;
    scaleFactor = 1.0;
    loadedFileName = fileName;
    setWindowFilePath(fileName);
    updateActions();
    updateDisplay();

    return true;
}

bool AnnotationVisualizer::loadAnnotations(const QDir& annDir) {
    spAnnotationData = new char ***[annotatorsList.size()];

    for (int i = 0; i < annotatorsList.size(); i++) {
        spAnnotationData[i] = new char **[slicesNo];
        for (int j = 0; j < slicesNo; j++) {
            spAnnotationData[i][j] = new char *[imageWidth];

            for (int k = 0; k < imageWidth; k++)
                spAnnotationData[i][j][k] = new char[imageHeight];
        }
    }

    manualCorrectionsData = new char ***[annotatorsList.size()];

    for (int i = 0; i < annotatorsList.size(); i++) {
        manualCorrectionsData[i] = new char **[slicesNo];
        for (int j = 0; j < slicesNo; j++) {
            manualCorrectionsData[i][j] = new char *[imageWidth];

            for (int k = 0; k < imageWidth; k++)
                manualCorrectionsData[i][j][k] = new char[imageHeight];
        }
    }

    QString spNumberVal;
    if (spNumber == "LOWER") {
        spNumberVal = imageType == "SPA" ? "1000" : "250";
    } else {
        spNumberVal = imageType == "SPA" ? "2000" : "500";
    }

    QString fileNameToLoad;

    QDir currDir;
    for (int ann_no = 0; ann_no < annotatorsList.size(); ann_no++) {
        currDir = annDir;
        if (!currDir.cd(annotatorsList.at(ann_no) + "/sp/" + imageType + spNumberVal + segmentationMethod)) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Cannot find superpixel annotations for %0%1 made by %2!")
                                             .arg(segmentationMethod).arg(spNumberVal).arg(annotatorsList.at(ann_no)));
            continue;
        }

        fileNameToLoad = currDir.path() + QString(QDir::separator()) + QString("%0spAnnotations%1_%2_%3_%4_%5_1_.raw")
                .arg(spNumberVal).arg(segmentationMethod).arg(patientNo).arg(imageWidth).arg(imageHeight).arg(slicesNo);

        if (!loadRaw(fileNameToLoad, spAnnotationData[ann_no])){
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Cannot load superpixel annotations for %0%1 made by %2!")
                                             .arg(segmentationMethod).arg(spNumberVal).arg(annotatorsList.at(ann_no)));
            continue;
        }

        if (!currDir.cd("../../manual/" + imageType + spNumberVal + segmentationMethod)) {
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Cannot find superpixel annotations for %0%1 made by %2!")
                                             .arg(segmentationMethod).arg(spNumberVal).arg(annotatorsList.at(ann_no)));
            continue;
        }

        fileNameToLoad = currDir.path() + QString(QDir::separator()) + QString("%0manualAnnotations%1_%2_%3_%4_%5_1_.raw")
                .arg(spNumberVal).arg(segmentationMethod).arg(patientNo).arg(imageWidth).arg(imageHeight).arg(slicesNo);

        if (!loadRaw(fileNameToLoad, manualCorrectionsData[ann_no])){
            QMessageBox::information(this, QGuiApplication::applicationDisplayName(),
                                     tr("Cannot load manual corrections for %0%1 made by %2!")
                                             .arg(segmentationMethod).arg(spNumberVal).arg(annotatorsList.at(ann_no)));
            continue;
        }
    }
    return true;
}

bool AnnotationVisualizer::loadRaw(const QString &fileName, unsigned short ***dataArray) const {
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

bool AnnotationVisualizer::loadRaw(const QString &fileName, char ***dataArray) const {
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

bool AnnotationVisualizer::loadRaw(const QString &fileName, bool ***dataArray) const {
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

void AnnotationVisualizer::updateDisplay() {
    QPixmap display(imageWidth, imageHeight);
    QPainter painter(&display);

    QImage stirImage (imageWidth, imageHeight, QImage::Format_RGBA64);
    QImage gridImage (imageWidth, imageHeight, QImage::Format_RGBA64);
    QImage annotationImage (imageWidth, imageHeight, QImage::Format_RGBA64);

    QRgba64 colorValue = {};

    unsigned short maxVal {1};
    for (int x = 0; x < imageWidth; x++)
        for (int y = 0; y < imageHeight; y++)
            if (maxVal < stirData[currSlice][x][y]){ maxVal=stirData[currSlice][x][y];}

    unsigned short val;
    for (int x = 0; x < imageWidth; x++)
        for (int y = 0; y < imageHeight; y++) {
            val = 65535 / maxVal * stirData[currSlice][x][y];
            colorValue = qRgba64(val, val, val, 65535);
            stirImage.setPixelColor(x, y, colorValue);
        }
    painter.drawImage(QPoint(0,0), stirImage);

    if(!annotationsHidden) {
        int displayedAnnotationsNo = 0;

        char **combinedAnnotationData;
        combinedAnnotationData = new char *[imageWidth];
        for (int x = 0; x < imageWidth; x++) {
            combinedAnnotationData[x] = new char[imageHeight];
            for (int y = 0; y < imageHeight; y++) {
                combinedAnnotationData[x][y] = 0;
            }
        }

        for (int ann_no = 0; ann_no < annotatorsList.size(); ann_no++) {
            if(annotatorsChoiceGroup->actions().at(ann_no)->isChecked()){
                displayedAnnotationsNo ++;
            } else {
                continue;
            }

            if (displayedAnnotations == "BOTH") {
                for (int x = 0; x < imageWidth; x++)
                    for (int y = 0; y < imageHeight; y++) {
                        if (spAnnotationData[ann_no][currSlice][x][y] + manualCorrectionsData[ann_no][currSlice][x][y] > 0) {
                            combinedAnnotationData[x][y]++;
                        }
                    }
            } else if (displayedAnnotations == "SP") {
                for (int x = 0; x < imageWidth; x++)
                    for (int y = 0; y < imageHeight; y++) {
                        if (spAnnotationData[ann_no][currSlice][x][y] == 1) {
                            combinedAnnotationData[x][y]++;
                        }
                    }
            } else {
                for (int x = 0; x < imageWidth; x++)
                    for (int y = 0; y < imageHeight; y++) {
                        if (manualCorrectionsData[ann_no][currSlice][x][y] == 1) {
                            combinedAnnotationData[x][y]++;
                        }
                    }
            }
        }

        // Generate heatmap
        for (int x = 0; x < imageWidth; x++)
            for (int y = 0; y < imageHeight; y++) {
                if (combinedAnnotationData[x][y] > 0) {
                    double red{1}, green{1}, blue{1};
                    if (combinedAnnotationData[x][y] < (1. + 0.25 * (displayedAnnotationsNo - 1.))) {
                        red = 0;
                        green = 4 * (combinedAnnotationData[x][y] - 1.) / (displayedAnnotationsNo - 1.);
                    } else if (combinedAnnotationData[x][y] < (1. + 0.5 * (displayedAnnotationsNo - 1.))) {
                        red = 0;
                        blue = 1 + 4 * (1. + 0.25 * (displayedAnnotationsNo - 1.) - combinedAnnotationData[x][y]) / (displayedAnnotationsNo - 1.);
                    } else if (combinedAnnotationData[x][y] < (1. + 0.75 * (displayedAnnotationsNo - 1.))) {
                        red = 4 * (combinedAnnotationData[x][y] - 1. - 0.5 * (displayedAnnotationsNo - 1.)) / (displayedAnnotationsNo - 1.);
                        blue = 0;
                    } else {
                        green = 1 + 4 * (1. + 0.75 * (displayedAnnotationsNo - 1.) - combinedAnnotationData[x][y]) / (displayedAnnotationsNo - 1.);
                        blue = 0;
                    }
                    colorValue = qRgba64(65535 * red, 65535 * green, 65535 * blue, 32767);
                } else {
                    colorValue = qRgba64(0, 0, 0, 0);
                }
                annotationImage.setPixelColor(x, y, colorValue);
            }

        painter.drawImage(QPoint(0,0), annotationImage);

        for (int x = 0; x < imageWidth; x++) {
            delete[] combinedAnnotationData[x];
        }
        delete[] combinedAnnotationData;

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
    scaleImage(1);
    scrollArea->horizontalScrollBar()->setValue(horizontalScrollValue);
    scrollArea->verticalScrollBar()->setValue(verticalScrollValue);
}

void AnnotationVisualizer::open() {
    QFileDialog dialog(this, tr("Open File"));
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
    }
}

void AnnotationVisualizer::closeImg() {
    setWindowFilePath("");
    imageLabel->setPixmap(QPixmap());
    loadedFileName = "";
    updateActions();
}

void AnnotationVisualizer::chooseSegmentationMethod(QAction* chooseMethodAct) {
    if (segmentationMethod != chooseMethodAct->data().toString()){
        segmentationMethod = chooseMethodAct->data().toString();
        if (loadedFileName != "") {
            QString fileNameToRemember = loadedFileName;
            closeImg();
            loadFiles(fileNameToRemember);
        }
    }
}

void AnnotationVisualizer::chooseSPNumber(QAction *chooseSPNumberAct) {
    if (spNumber != chooseSPNumberAct->data().toString()){
        spNumber = chooseSPNumberAct->data().toString();
        if (loadedFileName != "") {
            QString fileNameToRemember = loadedFileName;
            closeImg();
            loadFiles(fileNameToRemember);
        }
    }
}

void AnnotationVisualizer::zoomIn()
{
    scaleImage(1.25);
}

void AnnotationVisualizer::zoomOut()
{
    scaleImage(0.8);
}

void AnnotationVisualizer::resetSize()
{
    imageLabel->adjustSize();
    scaleFactor = 1.0;
}

void AnnotationVisualizer::scaleImage(double factor)
{
    scaleFactor *= factor;
    imageLabel->resize(scaleFactor * imageLabel->pixmap(Qt::ReturnByValue).size());

    adjustScrollBar(scrollArea->horizontalScrollBar(), factor);
    adjustScrollBar(scrollArea->verticalScrollBar(), factor);

    zoomInAct->setEnabled(scaleFactor < 5.0);
    zoomOutAct->setEnabled(scaleFactor > 0.2);
}

void AnnotationVisualizer::adjustScrollBar(QScrollBar *scrollBar, double factor)
{
    scrollBar->setValue(int(factor * scrollBar->value() + ((factor - 1) * scrollBar->pageStep()/2)));
}

void AnnotationVisualizer::nextSlice() {
    if (currSlice<slicesNo-1) {currSlice++;}
    updateDisplay();
}

void AnnotationVisualizer::previousSlice() {
    if (currSlice>0) {currSlice--;}
    updateDisplay();
}

void AnnotationVisualizer::changeDisplayGrid() {
    displayGrid = !displayGrid;
    updateDisplay();
}

void AnnotationVisualizer::hideAnnotations() {
    annotationsHidden = !annotationsHidden;
    updateDisplay();
}

void AnnotationVisualizer::changeDisplayedAnnotations(QAction* changeDisplayedAnnotationsAct) {
    if (displayedAnnotations != changeDisplayedAnnotationsAct->data().toString()){
        displayedAnnotations = changeDisplayedAnnotationsAct->data().toString();
        updateDisplay();
    }
}

void AnnotationVisualizer::instructions() {
    QMessageBox::about(this, tr("Instructions"),
                       tr("<p><b>Instructions:</b></p>"
                          "<p> TODO </p>"
                       ));
}
