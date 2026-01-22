#include "keymapeditor.h"
#include <QPainter>
#include <QMouseEvent>
#include <QMenu>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QDebug>
#include <QContextMenuEvent>
#include <QCoreApplication>

// ============================================================================
// KeyMapWidget Implementation
// ============================================================================

KeyMapWidget::KeyMapWidget(QWidget *parent)
    : QWidget(parent)
    , m_position(0.5, 0.5)
    , m_type(MT_CLICK)
    , m_selected(false)
    , m_dragging(false)
{
    setFixedSize(60, 60);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
}

void KeyMapWidget::setPosition(const QPointF &pos)
{
    m_position = pos;
    updateScreenSize(m_screenSize);
}

void KeyMapWidget::setMapType(MapType type)
{
    m_type = type;
    update();
}

void KeyMapWidget::setKeyBinding(const QString &key)
{
    m_keyBinding = key;
    update();
}

void KeyMapWidget::setComment(const QString &comment)
{
    m_comment = comment;
    update();
}

void KeyMapWidget::setData(const QJsonObject &data)
{
    m_data = data;
}

void KeyMapWidget::updateScreenSize(const QSize &screenSize)
{
    m_screenSize = screenSize;
    if (screenSize.isValid()) {
        int x = static_cast<int>(m_position.x() * screenSize.width()) - width() / 2;
        int y = static_cast<int>(m_position.y() * screenSize.height()) - height() / 2;
        move(x, y);
    }
}

void KeyMapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw background circle
    QColor bgColor = getTypeColor();
    if (m_selected) {
        bgColor = bgColor.lighter(120);
    }

    painter.setBrush(QBrush(bgColor));
    painter.setPen(QPen(Qt::white, 2));
    painter.drawEllipse(rect().adjusted(5, 5, -5, -5));

    // Draw type icon
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);

    QString icon = getTypeIcon();
    painter.drawText(rect(), Qt::AlignCenter, icon);

    // Draw key binding text
    if (!m_keyBinding.isEmpty()) {
        font.setPointSize(8);
        font.setBold(false);
        painter.setFont(font);
        QString displayKey = m_keyBinding;
        displayKey.replace("Key_", "");
        displayKey.replace("Button", "");
        painter.drawText(rect().adjusted(0, 35, 0, 0), Qt::AlignHCenter | Qt::AlignTop, displayKey);
    }

    // Draw comment tooltip
    if (!m_comment.isEmpty()) {
        setToolTip(m_comment + "\n" + m_keyBinding);
    }
}

void KeyMapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
        m_selected = true;
        emit selected(this);
        update();
    }
    QWidget::mousePressEvent(event);
}

void KeyMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        // Only start dragging if moved more than a threshold
        if (!m_dragging) {
            QPoint delta = event->pos() - m_dragStartPos;
            if (delta.manhattanLength() > 5) {
                m_dragging = true;
            }
        }

        if (m_dragging) {
            QPoint delta = event->pos() - m_dragStartPos;
            QPoint newPos = pos() + delta;

            // Keep within parent bounds
            if (parentWidget()) {
                newPos.setX(qBound(0, newPos.x(), parentWidget()->width() - width()));
                newPos.setY(qBound(0, newPos.y(), parentWidget()->height() - height()));
            }

            move(newPos);

            // Update percentage position
            if (m_screenSize.isValid()) {
                m_position.setX(static_cast<double>(newPos.x() + width() / 2) / m_screenSize.width());
                m_position.setY(static_cast<double>(newPos.y() + height() / 2) / m_screenSize.height());
                emit positionChanged(m_position);
            }
        }
    }
    QWidget::mouseMoveEvent(event);
}

void KeyMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void KeyMapWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *deleteAction = menu.addAction("Delete");
    QAction *selected = menu.exec(event->globalPos());

    if (selected == deleteAction) {
        emit deleted(this);
    }
}

QString KeyMapWidget::getTypeIcon() const
{
    switch (m_type) {
    case MT_CLICK:
        return "●";
    case MT_CLICK_TWICE:
        return "◎";
    case MT_CLICK_MULTI:
        return "⊕";
    case MT_STEER_WHEEL:
        return "⊗";
    case MT_DRAG:
        return "↔";
    case MT_MOUSE_MOVE:
        return "⊙";
    case MT_ANDROID_KEY:
        return "⌨";
    default:
        return "?";
    }
}

QColor KeyMapWidget::getTypeColor() const
{
    switch (m_type) {
    case MT_CLICK:
        return QColor(52, 152, 219, 180);      // Blue
    case MT_CLICK_TWICE:
        return QColor(155, 89, 182, 180);      // Purple
    case MT_CLICK_MULTI:
        return QColor(241, 196, 15, 180);      // Yellow
    case MT_STEER_WHEEL:
        return QColor(46, 204, 113, 180);      // Green
    case MT_DRAG:
        return QColor(230, 126, 34, 180);      // Orange
    case MT_MOUSE_MOVE:
        return QColor(231, 76, 60, 180);       // Red
    case MT_ANDROID_KEY:
        return QColor(149, 165, 166, 180);     // Gray
    default:
        return QColor(127, 127, 127, 180);
    }
}

// ============================================================================
// KeyMapEditor Implementation
// ============================================================================

KeyMapEditor::KeyMapEditor(QWidget *parent)
    : QWidget(parent)
    , m_selectedWidget(nullptr)
    , m_switchKey("Key_QuoteLeft")
{
    setupUI();
    resize(1400, 900);
    setWindowTitle("KeyMap Editor - QtScrcpy");
}

KeyMapEditor::~KeyMapEditor()
{
    qDeleteAll(m_keyMapWidgets);
}

void KeyMapEditor::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Setup toolbar
    setupToolbar();

    // Setup content area
    m_contentLayout = new QHBoxLayout();
    m_contentLayout->setSpacing(0);

    // Setup screen area
    setupScreenArea();

    // Setup properties panel
    setupPropertiesPanel();

    m_contentLayout->addWidget(m_screenScrollArea, 3);
    m_contentLayout->addWidget(m_propertiesPanel, 1);

    m_mainLayout->addWidget(m_toolbar);
    m_mainLayout->addLayout(m_contentLayout);
}

void KeyMapEditor::setupToolbar()
{
    m_toolbar = new QWidget(this);
    m_toolbar->setStyleSheet("QWidget { background-color: #2c3e50; } QPushButton { background-color: #34495e; color: white; border: none; padding: 8px 16px; margin: 4px; } QPushButton:hover { background-color: #3498db; }");

    QHBoxLayout *toolbarLayout = new QHBoxLayout(m_toolbar);

    m_btnNew = new QPushButton("New", m_toolbar);
    m_btnLoad = new QPushButton("Load", m_toolbar);
    m_btnSave = new QPushButton("Save", m_toolbar);
    m_btnExport = new QPushButton("Export", m_toolbar);
    m_btnImport = new QPushButton("Import", m_toolbar);
    m_btnCapture = new QPushButton("Capture Screen", m_toolbar);
    m_btnAddKey = new QPushButton("+ Add Key", m_toolbar);

    toolbarLayout->addWidget(m_btnNew);
    toolbarLayout->addWidget(m_btnLoad);
    toolbarLayout->addWidget(m_btnSave);
    toolbarLayout->addWidget(m_btnExport);
    toolbarLayout->addWidget(m_btnImport);
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(m_btnCapture);
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(m_btnAddKey);
    toolbarLayout->addStretch();

    connect(m_btnNew, &QPushButton::clicked, this, &KeyMapEditor::onNewKeyMap);
    connect(m_btnLoad, &QPushButton::clicked, this, &KeyMapEditor::onLoadKeyMap);
    connect(m_btnSave, &QPushButton::clicked, this, &KeyMapEditor::onSaveKeyMap);
    connect(m_btnExport, &QPushButton::clicked, this, &KeyMapEditor::onExportKeyMap);
    connect(m_btnImport, &QPushButton::clicked, this, &KeyMapEditor::onImportKeyMap);
    connect(m_btnCapture, &QPushButton::clicked, this, &KeyMapEditor::onScreenshotCapture);
    connect(m_btnAddKey, &QPushButton::clicked, this, &KeyMapEditor::onAddKeyMap);
}

void KeyMapEditor::setupScreenArea()
{
    m_screenScrollArea = new QScrollArea(this);
    m_screenScrollArea->setWidgetResizable(false);
    m_screenScrollArea->setStyleSheet("QScrollArea { background-color: #34495e; border: none; }");

    m_screenLabel = new QLabel(m_screenScrollArea);
    m_screenLabel->setAlignment(Qt::AlignCenter);
    m_screenLabel->setStyleSheet("QLabel { background-color: #2c3e50; }");
    m_screenLabel->setText("No phone screenshot loaded\n\nClick 'Capture Screen' or load an image");
    m_screenLabel->setMinimumSize(400, 800);

    m_screenScrollArea->setWidget(m_screenLabel);
}

void KeyMapEditor::setupPropertiesPanel()
{
    m_propertiesPanel = new QWidget(this);
    m_propertiesPanel->setStyleSheet(
        "QWidget { background-color: #ecf0f1; color: #2c3e50; } "
        "QLabel { color: #2c3e50; } "
        "QLineEdit { background-color: white; color: #2c3e50; border: 1px solid #bdc3c7; padding: 4px; } "
        "QComboBox { background-color: white; color: #2c3e50; border: 1px solid #bdc3c7; padding: 4px; } "
        "QSpinBox, QDoubleSpinBox { background-color: white; color: #2c3e50; border: 1px solid #bdc3c7; padding: 4px; } "
        "QCheckBox { color: #2c3e50; } "
        "QGroupBox { color: #2c3e50; font-weight: bold; border: 1px solid #bdc3c7; border-radius: 4px; margin-top: 8px; padding-top: 8px; } "
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 5px; }"
    );
    m_propertiesPanel->setMinimumWidth(350);
    m_propertiesPanel->setMaximumWidth(450);

    m_propertiesLayout = new QVBoxLayout(m_propertiesPanel);

    // Title
    QLabel *titleLabel = new QLabel("Key Mappings", m_propertiesPanel);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("QLabel { color: #2c3e50; padding: 10px; }");
    m_propertiesLayout->addWidget(titleLabel);

    // Switch Key
    QGroupBox *switchGroup = new QGroupBox("Switch Key (Toggle KeyMap)", m_propertiesPanel);
    QHBoxLayout *switchLayout = new QHBoxLayout(switchGroup);
    m_switchKeyEdit = new QLineEdit(m_switchKey, switchGroup);
    m_switchKeyEdit->setPlaceholderText("e.g., Key_QuoteLeft");
    switchLayout->addWidget(new QLabel("Key:"));
    switchLayout->addWidget(m_switchKeyEdit);
    m_propertiesLayout->addWidget(switchGroup);

    // Key Map List
    QLabel *listLabel = new QLabel("Key Map Nodes:", m_propertiesPanel);
    listLabel->setStyleSheet("QLabel { font-weight: bold; padding: 5px; }");
    m_propertiesLayout->addWidget(listLabel);

    m_keyMapList = new QListWidget(m_propertiesPanel);
    m_keyMapList->setStyleSheet("QListWidget { background-color: white; border: 1px solid #bdc3c7; }");
    m_propertiesLayout->addWidget(m_keyMapList);

    connect(m_keyMapList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < m_keyMapWidgets.size()) {
            onKeyMapSelected(m_keyMapWidgets[row]);
        }
    });

    // Properties editor
    QGroupBox *propsGroup = new QGroupBox("Selected Key Properties", m_propertiesPanel);

    // Add scroll area for properties
    QScrollArea *propsScrollArea = new QScrollArea(propsGroup);
    propsScrollArea->setWidgetResizable(true);
    propsScrollArea->setFrameShape(QFrame::NoFrame);
    propsScrollArea->setStyleSheet("QScrollArea { background-color: transparent; border: none; }");

    QWidget *propsScrollWidget = new QWidget();
    QFormLayout *formLayout = new QFormLayout(propsScrollWidget);
    propsScrollArea->setWidget(propsScrollWidget);

    QVBoxLayout *propsGroupLayout = new QVBoxLayout(propsGroup);
    propsGroupLayout->addWidget(propsScrollArea);

    m_typeCombo = new QComboBox(propsGroup);
    m_typeCombo->addItem("Click", KeyMapWidget::MT_CLICK);
    m_typeCombo->addItem("Click Twice", KeyMapWidget::MT_CLICK_TWICE);
    m_typeCombo->addItem("Click Multi", KeyMapWidget::MT_CLICK_MULTI);
    m_typeCombo->addItem("Steer Wheel", KeyMapWidget::MT_STEER_WHEEL);
    m_typeCombo->addItem("Drag", KeyMapWidget::MT_DRAG);
    m_typeCombo->addItem("Mouse Move", KeyMapWidget::MT_MOUSE_MOVE);
    m_typeCombo->addItem("Android Key", KeyMapWidget::MT_ANDROID_KEY);

    m_keyBindingEdit = new QLineEdit(propsGroup);
    m_keyBindingEdit->setPlaceholderText("e.g., Key_A, LeftButton");

    m_commentEdit = new QLineEdit(propsGroup);
    m_commentEdit->setPlaceholderText("Description");

    m_posXEdit = new QLineEdit(propsGroup);
    m_posXEdit->setPlaceholderText("0.0 - 1.0");

    m_posYEdit = new QLineEdit(propsGroup);
    m_posYEdit->setPlaceholderText("0.0 - 1.0");

    formLayout->addRow("Type:", m_typeCombo);
    formLayout->addRow("Key Binding:", m_keyBindingEdit);
    formLayout->addRow("Comment:", m_commentEdit);
    formLayout->addRow("Position X:", m_posXEdit);
    formLayout->addRow("Position Y:", m_posYEdit);

    // Additional properties area
    m_additionalPropsWidget = new QWidget(propsGroup);
    m_additionalPropsLayout = new QVBoxLayout(m_additionalPropsWidget);
    formLayout->addRow(m_additionalPropsWidget);

    m_btnDelete = new QPushButton("Delete Selected", propsGroup);
    m_btnDelete->setStyleSheet("QPushButton { background-color: #e74c3c; color: white; padding: 8px; } QPushButton:hover { background-color: #c0392b; }");
    formLayout->addRow(m_btnDelete);

    m_propertiesLayout->addWidget(propsGroup);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KeyMapEditor::onTypeChanged);
    connect(m_keyBindingEdit, &QLineEdit::textChanged, this, &KeyMapEditor::onKeyBindingChanged);
    connect(m_commentEdit, &QLineEdit::textChanged, this, &KeyMapEditor::onCommentChanged);
    connect(m_posXEdit, &QLineEdit::textChanged, this, &KeyMapEditor::onPositionChanged);
    connect(m_posYEdit, &QLineEdit::textChanged, this, &KeyMapEditor::onPositionChanged);
    connect(m_btnDelete, &QPushButton::clicked, this, &KeyMapEditor::onDeleteKeyMap);

    clearPropertiesPanel();
}

void KeyMapEditor::setPhoneScreenshot(const QPixmap &screenshot)
{
    m_phoneScreenshot = screenshot;
    m_screenLabel->setPixmap(screenshot);
    m_screenLabel->setFixedSize(screenshot.size());

    // Update all widget positions
    for (KeyMapWidget *widget : m_keyMapWidgets) {
        widget->updateScreenSize(screenshot.size());
    }
}

void KeyMapEditor::onAddKeyMap()
{
    KeyMapWidget *widget = new KeyMapWidget(m_screenLabel);
    widget->setPosition(QPointF(0.5, 0.5));
    widget->setMapType(KeyMapWidget::MT_CLICK);
    widget->setKeyBinding("Key_A");
    widget->setComment("New Key");
    widget->updateScreenSize(m_phoneScreenshot.size());
    widget->show();

    connect(widget, &KeyMapWidget::selected, this, &KeyMapEditor::onKeyMapSelected);
    connect(widget, &KeyMapWidget::deleted, this, &KeyMapEditor::onDeleteKeyMap);
    connect(widget, &KeyMapWidget::positionChanged, this, [this](const QPointF &pos) {
        if (m_selectedWidget && sender() == m_selectedWidget) {
            // Update position fields in real-time during drag
            m_posXEdit->blockSignals(true);
            m_posYEdit->blockSignals(true);
            m_posXEdit->setText(QString::number(pos.x(), 'f', 3));
            m_posYEdit->setText(QString::number(pos.y(), 'f', 3));
            m_posXEdit->blockSignals(false);
            m_posYEdit->blockSignals(false);
        }
    });

    m_keyMapWidgets.append(widget);

    // Add to list
    m_keyMapList->addItem(widget->comment() + " - " + widget->keyBinding());

    // Select the new widget
    onKeyMapSelected(widget);
}

void KeyMapEditor::onDeleteKeyMap()
{
    if (m_selectedWidget) {
        m_keyMapWidgets.removeOne(m_selectedWidget);

        // Remove from list
        for (int i = 0; i < m_keyMapList->count(); ++i) {
            if (m_keyMapList->item(i)->text().contains(m_selectedWidget->keyBinding())) {
                delete m_keyMapList->takeItem(i);
                break;
            }
        }

        m_selectedWidget->deleteLater();
        m_selectedWidget = nullptr;
        clearPropertiesPanel();
    }
}

void KeyMapEditor::onKeyMapSelected(KeyMapWidget *widget)
{
    // Prevent re-entry
    static bool isUpdating = false;
    if (isUpdating) {
        return;
    }
    isUpdating = true;

    // Deselect previous
    if (m_selectedWidget) {
        m_selectedWidget->update();
    }

    m_selectedWidget = widget;
    updatePropertiesPanel();

    isUpdating = false;
}

void KeyMapEditor::updatePropertiesPanel()
{
    // Prevent re-entry
    static bool isUpdating = false;
    if (isUpdating) {
        return;
    }
    isUpdating = true;

    if (!m_selectedWidget) {
        clearPropertiesPanel();
        isUpdating = false;
        return;
    }

    // Block signals to prevent triggering change handlers
    m_typeCombo->blockSignals(true);
    m_keyBindingEdit->blockSignals(true);
    m_commentEdit->blockSignals(true);
    m_posXEdit->blockSignals(true);
    m_posYEdit->blockSignals(true);

    m_typeCombo->setCurrentIndex(m_selectedWidget->mapType());
    m_keyBindingEdit->setText(m_selectedWidget->keyBinding());
    m_commentEdit->setText(m_selectedWidget->comment());
    m_posXEdit->setText(QString::number(m_selectedWidget->position().x(), 'f', 3));
    m_posYEdit->setText(QString::number(m_selectedWidget->position().y(), 'f', 3));

    m_typeCombo->setEnabled(true);
    m_keyBindingEdit->setEnabled(true);
    m_commentEdit->setEnabled(true);
    m_posXEdit->setEnabled(true);
    m_posYEdit->setEnabled(true);
    m_btnDelete->setEnabled(true);

    // Unblock signals
    m_typeCombo->blockSignals(false);
    m_keyBindingEdit->blockSignals(false);
    m_commentEdit->blockSignals(false);
    m_posXEdit->blockSignals(false);
    m_posYEdit->blockSignals(false);

    // Update type-specific properties
    updateTypeSpecificProperties(m_selectedWidget->mapType());

    isUpdating = false;
}

void KeyMapEditor::clearPropertiesPanel()
{
    m_typeCombo->setCurrentIndex(0);
    m_keyBindingEdit->clear();
    m_commentEdit->clear();
    m_posXEdit->clear();
    m_posYEdit->clear();

    m_typeCombo->setEnabled(false);
    m_keyBindingEdit->setEnabled(false);
    m_commentEdit->setEnabled(false);
    m_posXEdit->setEnabled(false);
    m_posYEdit->setEnabled(false);
    m_btnDelete->setEnabled(false);

    // Clear type-specific properties
    clearTypeSpecificProperties();
}

void KeyMapEditor::onTypeChanged(int index)
{
    if (m_selectedWidget) {
        KeyMapWidget::MapType type = static_cast<KeyMapWidget::MapType>(index);
        m_selectedWidget->setMapType(type);
        updateTypeSpecificProperties(type);
    }
}

void KeyMapEditor::clearTypeSpecificProperties()
{
    // Prevent re-entry
    static bool isClearing = false;
    if (isClearing) {
        return;
    }
    isClearing = true;

    // Clear all type-specific widgets first
    m_typeSpecificWidgets.clear();

    // Take all items from the layout
    while (m_additionalPropsLayout->count() > 0) {
        QLayoutItem *item = m_additionalPropsLayout->takeAt(0);
        if (!item) {
            break;
        }

        if (item->widget()) {
            QWidget *widget = item->widget();
            widget->setParent(nullptr);
            widget->deleteLater();
        } else if (item->layout()) {
            QLayout *layout = item->layout();
            // Recursively clear the nested layout
            while (layout->count() > 0) {
                QLayoutItem *subItem = layout->takeAt(0);
                if (!subItem) {
                    break;
                }

                if (subItem->widget()) {
                    QWidget *subWidget = subItem->widget();
                    subWidget->setParent(nullptr);
                    subWidget->deleteLater();
                }
                delete subItem;
            }
            // Don't delete layout immediately - let Qt handle it
            layout->setParent(nullptr);
        }
        delete item;
    }

    isClearing = false;
}

void KeyMapEditor::updateTypeSpecificProperties(KeyMapWidget::MapType type)
{
    clearTypeSpecificProperties();

    QFormLayout *formLayout = new QFormLayout();
    m_additionalPropsLayout->addLayout(formLayout);

    switch (type) {
    case KeyMapWidget::MT_CLICK: {
        // Click: switchMap, resetMap
        QCheckBox *switchMapCheck = new QCheckBox();
        QCheckBox *resetMapCheck = new QCheckBox();
        formLayout->addRow("Switch Map:", switchMapCheck);
        formLayout->addRow("Reset Map:", resetMapCheck);
        m_typeSpecificWidgets["switchMap"] = switchMapCheck;
        m_typeSpecificWidgets["resetMap"] = resetMapCheck;

        // Load existing data
        if (m_selectedWidget && m_selectedWidget->data().contains("switchMap")) {
            switchMapCheck->setChecked(m_selectedWidget->data()["switchMap"].toBool());
        }
        if (m_selectedWidget && m_selectedWidget->data().contains("resetMap")) {
            resetMapCheck->setChecked(m_selectedWidget->data()["resetMap"].toBool());
        }
        break;
    }
    case KeyMapWidget::MT_CLICK_TWICE: {
        // Click Twice: no additional parameters
        QLabel *infoLabel = new QLabel("Double click at the position");
        infoLabel->setWordWrap(true);
        formLayout->addRow(infoLabel);
        break;
    }
    case KeyMapWidget::MT_CLICK_MULTI: {
        // Click Multi: multiple click positions
        QLabel *infoLabel = new QLabel("Multiple click positions (TODO: Add UI for multiple positions)");
        infoLabel->setWordWrap(true);
        formLayout->addRow(infoLabel);
        break;
    }
    case KeyMapWidget::MT_STEER_WHEEL: {
        // Steer Wheel: centerPos, leftOffset, rightOffset, upOffset, downOffset, leftKey, rightKey, upKey, downKey
        QDoubleSpinBox *leftOffset = new QDoubleSpinBox();
        QDoubleSpinBox *rightOffset = new QDoubleSpinBox();
        QDoubleSpinBox *upOffset = new QDoubleSpinBox();
        QDoubleSpinBox *downOffset = new QDoubleSpinBox();
        QLineEdit *leftKey = new QLineEdit();
        QLineEdit *rightKey = new QLineEdit();
        QLineEdit *upKey = new QLineEdit();
        QLineEdit *downKey = new QLineEdit();

        leftOffset->setRange(0.0, 1.0);
        rightOffset->setRange(0.0, 1.0);
        upOffset->setRange(0.0, 1.0);
        downOffset->setRange(0.0, 1.0);
        leftOffset->setSingleStep(0.01);
        rightOffset->setSingleStep(0.01);
        upOffset->setSingleStep(0.01);
        downOffset->setSingleStep(0.01);
        leftOffset->setDecimals(3);
        rightOffset->setDecimals(3);
        upOffset->setDecimals(3);
        downOffset->setDecimals(3);

        leftOffset->setValue(0.1);
        rightOffset->setValue(0.1);
        upOffset->setValue(0.1);
        downOffset->setValue(0.1);

        leftKey->setPlaceholderText("e.g., Key_A");
        rightKey->setPlaceholderText("e.g., Key_D");
        upKey->setPlaceholderText("e.g., Key_W");
        downKey->setPlaceholderText("e.g., Key_S");

        formLayout->addRow("Left Offset:", leftOffset);
        formLayout->addRow("Right Offset:", rightOffset);
        formLayout->addRow("Up Offset:", upOffset);
        formLayout->addRow("Down Offset:", downOffset);
        formLayout->addRow("Left Key:", leftKey);
        formLayout->addRow("Right Key:", rightKey);
        formLayout->addRow("Up Key:", upKey);
        formLayout->addRow("Down Key:", downKey);

        m_typeSpecificWidgets["leftOffset"] = leftOffset;
        m_typeSpecificWidgets["rightOffset"] = rightOffset;
        m_typeSpecificWidgets["upOffset"] = upOffset;
        m_typeSpecificWidgets["downOffset"] = downOffset;
        m_typeSpecificWidgets["leftKey"] = leftKey;
        m_typeSpecificWidgets["rightKey"] = rightKey;
        m_typeSpecificWidgets["upKey"] = upKey;
        m_typeSpecificWidgets["downKey"] = downKey;

        // Load existing data
        if (m_selectedWidget) {
            QJsonObject data = m_selectedWidget->data();
            if (data.contains("leftOffset")) leftOffset->setValue(data["leftOffset"].toDouble());
            if (data.contains("rightOffset")) rightOffset->setValue(data["rightOffset"].toDouble());
            if (data.contains("upOffset")) upOffset->setValue(data["upOffset"].toDouble());
            if (data.contains("downOffset")) downOffset->setValue(data["downOffset"].toDouble());
            if (data.contains("leftKey")) leftKey->setText(data["leftKey"].toString());
            if (data.contains("rightKey")) rightKey->setText(data["rightKey"].toString());
            if (data.contains("upKey")) upKey->setText(data["upKey"].toString());
            if (data.contains("downKey")) downKey->setText(data["downKey"].toString());
        }
        break;
    }
    case KeyMapWidget::MT_DRAG: {
        // Drag: startPos, endPos
        QLineEdit *startX = new QLineEdit();
        QLineEdit *startY = new QLineEdit();
        QLineEdit *endX = new QLineEdit();
        QLineEdit *endY = new QLineEdit();

        startX->setPlaceholderText("0.0 - 1.0");
        startY->setPlaceholderText("0.0 - 1.0");
        endX->setPlaceholderText("0.0 - 1.0");
        endY->setPlaceholderText("0.0 - 1.0");

        formLayout->addRow("Start X:", startX);
        formLayout->addRow("Start Y:", startY);
        formLayout->addRow("End X:", endX);
        formLayout->addRow("End Y:", endY);

        m_typeSpecificWidgets["startX"] = startX;
        m_typeSpecificWidgets["startY"] = startY;
        m_typeSpecificWidgets["endX"] = endX;
        m_typeSpecificWidgets["endY"] = endY;

        // Load existing data
        if (m_selectedWidget) {
            QJsonObject data = m_selectedWidget->data();
            if (data.contains("startPos") && data["startPos"].isObject()) {
                QJsonObject startPos = data["startPos"].toObject();
                startX->setText(QString::number(startPos["x"].toDouble(), 'f', 3));
                startY->setText(QString::number(startPos["y"].toDouble(), 'f', 3));
            }
            if (data.contains("endPos") && data["endPos"].isObject()) {
                QJsonObject endPos = data["endPos"].toObject();
                endX->setText(QString::number(endPos["x"].toDouble(), 'f', 3));
                endY->setText(QString::number(endPos["y"].toDouble(), 'f', 3));
            }
        }
        break;
    }
    case KeyMapWidget::MT_MOUSE_MOVE: {
        // Mouse Move: startPos, speedRatioX, speedRatioY
        QLineEdit *startX = new QLineEdit();
        QLineEdit *startY = new QLineEdit();
        QDoubleSpinBox *speedRatioX = new QDoubleSpinBox();
        QDoubleSpinBox *speedRatioY = new QDoubleSpinBox();

        startX->setPlaceholderText("0.0 - 1.0");
        startY->setPlaceholderText("0.0 - 1.0");
        speedRatioX->setRange(0.0, 20.0);
        speedRatioY->setRange(0.0, 20.0);
        speedRatioX->setSingleStep(0.25);
        speedRatioY->setSingleStep(0.25);
        speedRatioX->setDecimals(2);
        speedRatioY->setDecimals(2);
        speedRatioX->setValue(1.0);
        speedRatioY->setValue(1.0);

        formLayout->addRow("Start X:", startX);
        formLayout->addRow("Start Y:", startY);
        formLayout->addRow("Speed Ratio X:", speedRatioX);
        formLayout->addRow("Speed Ratio Y:", speedRatioY);

        m_typeSpecificWidgets["startX"] = startX;
        m_typeSpecificWidgets["startY"] = startY;
        m_typeSpecificWidgets["speedRatioX"] = speedRatioX;
        m_typeSpecificWidgets["speedRatioY"] = speedRatioY;

        // Load existing data
        if (m_selectedWidget) {
            QJsonObject data = m_selectedWidget->data();
            if (data.contains("startPos") && data["startPos"].isObject()) {
                QJsonObject startPos = data["startPos"].toObject();
                startX->setText(QString::number(startPos["x"].toDouble(), 'f', 3));
                startY->setText(QString::number(startPos["y"].toDouble(), 'f', 3));
            }
            // speedRatioX and speedRatioY are separate values, not in an object
            if (data.contains("speedRatioX")) {
                speedRatioX->setValue(data["speedRatioX"].toDouble());
            }
            if (data.contains("speedRatioY")) {
                speedRatioY->setValue(data["speedRatioY"].toDouble());
            }
        }
        break;
    }
    case KeyMapWidget::MT_ANDROID_KEY: {
        // Android Key: androidKey code
        QSpinBox *androidKeyCode = new QSpinBox();
        androidKeyCode->setRange(0, 999);
        androidKeyCode->setValue(0);
        formLayout->addRow("Android Key Code:", androidKeyCode);
        m_typeSpecificWidgets["androidKey"] = androidKeyCode;

        // Load existing data
        if (m_selectedWidget && m_selectedWidget->data().contains("androidKey")) {
            androidKeyCode->setValue(m_selectedWidget->data()["androidKey"].toInt());
        }
        break;
    }
    default:
        break;
    }
}

void KeyMapEditor::onKeyBindingChanged()
{
    if (m_selectedWidget) {
        m_selectedWidget->setKeyBinding(m_keyBindingEdit->text());
    }
}

void KeyMapEditor::onCommentChanged()
{
    if (m_selectedWidget) {
        m_selectedWidget->setComment(m_commentEdit->text());
    }
}

void KeyMapEditor::onPositionChanged()
{
    if (m_selectedWidget) {
        bool okX, okY;
        double x = m_posXEdit->text().toDouble(&okX);
        double y = m_posYEdit->text().toDouble(&okY);
        if (okX && okY) {
            // Only update if position actually changed (avoid rounding errors causing jumps)
            QPointF currentPos = m_selectedWidget->position();
            if (qAbs(currentPos.x() - x) > 0.001 || qAbs(currentPos.y() - y) > 0.001) {
                m_selectedWidget->setPosition(QPointF(x, y));
            }
        }
    }
}

void KeyMapEditor::onNewKeyMap()
{
    qDeleteAll(m_keyMapWidgets);
    m_keyMapWidgets.clear();
    m_keyMapList->clear();
    m_selectedWidget = nullptr;
    m_currentFilePath.clear();
    clearPropertiesPanel();

    QMessageBox::information(this, "New KeyMap", "New keymap created. Add keys using the '+ Add Key' button.");
}

void KeyMapEditor::onLoadKeyMap()
{
    QString defaultPath = QCoreApplication::applicationDirPath() + "/../../../keymap";
    QString filePath = QFileDialog::getOpenFileName(this, "Load KeyMap", defaultPath, "JSON Files (*.json)");
    if (!filePath.isEmpty()) {
        loadKeyMap(filePath);
    }
}

void KeyMapEditor::onSaveKeyMap()
{
    if (m_currentFilePath.isEmpty()) {
        onExportKeyMap();
    } else {
        saveKeyMap(m_currentFilePath);
    }
}

void KeyMapEditor::onExportKeyMap()
{
    QString defaultPath = QCoreApplication::applicationDirPath() + "/../../../keymap";
    QString filePath = QFileDialog::getSaveFileName(this, "Export KeyMap", defaultPath, "JSON Files (*.json)");
    if (!filePath.isEmpty()) {
        saveKeyMap(filePath);
        m_currentFilePath = filePath;
    }
}

void KeyMapEditor::onImportKeyMap()
{
    onLoadKeyMap();
}

void KeyMapEditor::onScreenshotCapture()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Load Phone Screenshot", ".", "Images (*.png *.jpg *.jpeg *.bmp)");
    if (!filePath.isEmpty()) {
        QPixmap screenshot(filePath);
        if (!screenshot.isNull()) {
            setPhoneScreenshot(screenshot);
        } else {
            QMessageBox::warning(this, "Error", "Failed to load image");
        }
    }
}

void KeyMapEditor::loadKeyMap(const QString &jsonFilePath)
{
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Error", "Failed to open file: " + jsonFilePath);
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, "Error", "JSON parse error: " + error.errorString());
        return;
    }

    // Clear existing
    qDeleteAll(m_keyMapWidgets);
    m_keyMapWidgets.clear();
    m_keyMapList->clear();
    m_selectedWidget = nullptr;

    // Load from JSON
    loadFromJson(doc.object());

    m_currentFilePath = jsonFilePath;
    QMessageBox::information(this, "Success", "KeyMap loaded successfully!");
}

void KeyMapEditor::saveKeyMap(const QString &jsonFilePath)
{
    QJsonObject rootObj = createKeyMapJson();

    QJsonDocument doc(rootObj);
    QFile file(jsonFilePath);

    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "Failed to save file: " + jsonFilePath);
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    QMessageBox::information(this, "Success", "KeyMap saved successfully!");
    emit keyMapSaved(jsonFilePath);
}

QJsonObject KeyMapEditor::createKeyMapJson()
{
    QJsonObject rootObj;

    // Switch key
    rootObj["switchKey"] = m_switchKeyEdit->text();

    // Key map nodes
    QJsonArray nodesArray;
    for (KeyMapWidget *widget : m_keyMapWidgets) {
        QJsonObject node;
        node["comment"] = widget->comment();
        node["key"] = widget->keyBinding();

        QJsonObject pos;
        pos["x"] = widget->position().x();
        pos["y"] = widget->position().y();
        node["pos"] = pos;

        // Type
        switch (widget->mapType()) {
        case KeyMapWidget::MT_CLICK:
            node["type"] = "KMT_CLICK";
            node["switchMap"] = false;
            node["resetMap"] = false;
            break;
        case KeyMapWidget::MT_CLICK_TWICE:
            node["type"] = "KMT_CLICK_TWICE";
            break;
        case KeyMapWidget::MT_CLICK_MULTI:
            node["type"] = "KMT_CLICK_MULTI";
            node["clickNodes"] = QJsonArray(); // TODO: Add support for multiple clicks
            break;
        case KeyMapWidget::MT_STEER_WHEEL:
            node["type"] = "KMT_STEER_WHEEL";
            // TODO: Add steer wheel specific properties
            break;
        case KeyMapWidget::MT_DRAG:
            node["type"] = "KMT_DRAG";
            // TODO: Add drag specific properties
            break;
        case KeyMapWidget::MT_ANDROID_KEY:
            node["type"] = "KMT_ANDROID_KEY";
            node["androidKey"] = 0; // TODO: Add android key code
            break;
        default:
            continue;
        }

        nodesArray.append(node);
    }

    rootObj["keyMapNodes"] = nodesArray;

    return rootObj;
}

void KeyMapEditor::loadFromJson(const QJsonObject &rootObj)
{
    // Load switch key
    if (rootObj.contains("switchKey")) {
        m_switchKey = rootObj["switchKey"].toString();
        m_switchKeyEdit->setText(m_switchKey);
    }

    // Load key map nodes
    if (rootObj.contains("keyMapNodes") && rootObj["keyMapNodes"].isArray()) {
        QJsonArray nodes = rootObj["keyMapNodes"].toArray();

        for (const QJsonValue &value : nodes) {
            if (!value.isObject()) continue;

            QJsonObject node = value.toObject();
            addKeyMapWidget(node);
        }
    }
}

void KeyMapEditor::addKeyMapWidget(const QJsonObject &nodeData)
{
    KeyMapWidget *widget = new KeyMapWidget(m_screenLabel);

    // Parse type
    QString typeStr = nodeData["type"].toString();
    KeyMapWidget::MapType type = KeyMapWidget::MT_CLICK;
    if (typeStr == "KMT_CLICK") type = KeyMapWidget::MT_CLICK;
    else if (typeStr == "KMT_CLICK_TWICE") type = KeyMapWidget::MT_CLICK_TWICE;
    else if (typeStr == "KMT_CLICK_MULTI") type = KeyMapWidget::MT_CLICK_MULTI;
    else if (typeStr == "KMT_STEER_WHEEL") type = KeyMapWidget::MT_STEER_WHEEL;
    else if (typeStr == "KMT_DRAG") type = KeyMapWidget::MT_DRAG;
    else if (typeStr == "KMT_MOUSE_MOVE") type = KeyMapWidget::MT_MOUSE_MOVE;
    else if (typeStr == "KMT_ANDROID_KEY") type = KeyMapWidget::MT_ANDROID_KEY;

    widget->setMapType(type);
    widget->setKeyBinding(nodeData["key"].toString());
    widget->setComment(nodeData["comment"].toString());

    // Parse position
    if (nodeData.contains("pos") && nodeData["pos"].isObject()) {
        QJsonObject pos = nodeData["pos"].toObject();
        double x = pos["x"].toDouble(0.5);
        double y = pos["y"].toDouble(0.5);
        widget->setPosition(QPointF(x, y));
    } else if (nodeData.contains("centerPos") && nodeData["centerPos"].isObject()) {
        // For steer wheel
        QJsonObject pos = nodeData["centerPos"].toObject();
        double x = pos["x"].toDouble(0.5);
        double y = pos["y"].toDouble(0.5);
        widget->setPosition(QPointF(x, y));
    }

    widget->setData(nodeData);
    widget->updateScreenSize(m_phoneScreenshot.size());
    widget->show();

    connect(widget, &KeyMapWidget::selected, this, &KeyMapEditor::onKeyMapSelected);
    connect(widget, &KeyMapWidget::deleted, this, &KeyMapEditor::onDeleteKeyMap);
    connect(widget, &KeyMapWidget::positionChanged, this, [this](const QPointF &pos) {
        if (m_selectedWidget && sender() == m_selectedWidget) {
            // Update position fields in real-time during drag
            m_posXEdit->blockSignals(true);
            m_posYEdit->blockSignals(true);
            m_posXEdit->setText(QString::number(pos.x(), 'f', 3));
            m_posYEdit->setText(QString::number(pos.y(), 'f', 3));
            m_posXEdit->blockSignals(false);
            m_posYEdit->blockSignals(false);
        }
    });

    m_keyMapWidgets.append(widget);

    // Add to list
    QString listText = widget->comment();
    if (listText.isEmpty()) {
        listText = widget->keyBinding();
    } else {
        listText += " - " + widget->keyBinding();
    }
    m_keyMapList->addItem(listText);
}
