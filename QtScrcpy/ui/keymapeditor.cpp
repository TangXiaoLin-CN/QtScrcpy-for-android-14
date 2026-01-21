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
        m_dragging = true;
        m_dragStartPos = event->pos();
        m_selected = true;
        emit selected(this);
        update();
    }
    QWidget::mousePressEvent(event);
}

void KeyMapWidget::mouseMoveEvent(QMouseEvent *event)
{
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
    m_propertiesPanel->setStyleSheet("QWidget { background-color: #ecf0f1; }");
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

    // Properties editor
    QGroupBox *propsGroup = new QGroupBox("Selected Key Properties", m_propertiesPanel);
    QFormLayout *formLayout = new QFormLayout(propsGroup);

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
    // Deselect previous
    if (m_selectedWidget) {
        m_selectedWidget->update();
    }

    m_selectedWidget = widget;
    updatePropertiesPanel();
}

void KeyMapEditor::updatePropertiesPanel()
{
    if (!m_selectedWidget) {
        clearPropertiesPanel();
        return;
    }

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
}

void KeyMapEditor::onTypeChanged(int index)
{
    if (m_selectedWidget) {
        m_selectedWidget->setMapType(static_cast<KeyMapWidget::MapType>(index));
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
            m_selectedWidget->setPosition(QPointF(x, y));
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
    QString filePath = QFileDialog::getOpenFileName(this, "Load KeyMap", "./keymap", "JSON Files (*.json)");
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
    QString filePath = QFileDialog::getSaveFileName(this, "Export KeyMap", "./keymap", "JSON Files (*.json)");
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
