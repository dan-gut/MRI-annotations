#include <QApplication>
#include "annotationmanager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QGuiApplication::setApplicationDisplayName(AnnotationManager::tr("Annotation manager"));
    AnnotationManager annotationManager;
    annotationManager.show();
    return app.exec();
}
