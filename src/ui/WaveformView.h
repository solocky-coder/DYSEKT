#include <QWidget>

class WaveformView : public QWidget {
    Q_OBJECT

public:
    WaveformView(QWidget *parent = nullptr);
    ~WaveformView();

private:
    void setupUi();
};