#include <QDialog>
#include <QPushButton>
#include <QPainter>
#include <QVBoxLayout>

class TrimDialog : public QDialog {
    Q_OBJECT

public:
    explicit TrimDialog(QWidget *parent = nullptr);
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onYesClicked();
    void onNoClicked();

public:
    void show();
};

TrimDialog::TrimDialog(QWidget *parent) : QDialog(parent) {
    QPushButton *yesButton = new QPushButton("Yes", this);
    QPushButton *noButton = new QPushButton("No", this);
    connect(yesButton, &QPushButton::clicked, this, &TrimDialog::onYesClicked);
    connect(noButton, &QPushButton::clicked, this, &TrimDialog::onNoClicked);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(yesButton);
    layout->addWidget(noButton);
    setLayout(layout);
}

void TrimDialog::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    // Custom painting code
}

void TrimDialog::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    // Handle resize
}

void TrimDialog::onYesClicked() {
    // Yes clicked logic
}

void TrimDialog::onNoClicked() {
    // No clicked logic
}

void TrimDialog::show() {
    QDialog::show();
    // Additional show logic if needed
}