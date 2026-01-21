#ifndef KEYMAPEDITOR_H
#define KEYMAPEDITOR_H

#include <QWidget>
#include <QLabel>
#include <QPixmap>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QScrollArea>

// Forward declaration
class KeyMapNode;

// Visual representation of a key mapping on the screen
class KeyMapWidget : public QWidget
{
    Q_OBJECT
public:
    enum MapType {
        MT_CLICK,
        MT_CLICK_TWICE,
        MT_CLICK_MULTI,
        MT_STEER_WHEEL,
        MT_DRAG,
        MT_MOUSE_MOVE,
        MT_ANDROID_KEY
    };

    explicit KeyMapWidget(QWidget *parent = nullptr);

    void setPosition(const QPointF &pos); // Position in percentage (0.0-1.0)
    void setMapType(MapType type);
    void setKeyBinding(const QString &key);
    void setComment(const QString &comment);
    void setData(const QJsonObject &data);

    QPointF position() const { return m_position; }
    MapType mapType() const { return m_type; }
    QString keyBinding() const { return m_keyBinding; }
    QString comment() const { return m_comment; }
    QJsonObject data() const { return m_data; }

    void updateScreenSize(const QSize &screenSize);

signals:
    void selected(KeyMapWidget *widget);
    void positionChanged(const QPointF &pos);
    void deleted(KeyMapWidget *widget);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QPointF m_position;      // Percentage position (0.0-1.0)
    MapType m_type;
    QString m_keyBinding;
    QString m_comment;
    QJsonObject m_data;
    bool m_selected;
    bool m_dragging;
    QPoint m_dragStartPos;
    QSize m_screenSize;

    QString getTypeIcon() const;
    QColor getTypeColor() const;
};

// Main KeyMap Editor Window
class KeyMapEditor : public QWidget
{
    Q_OBJECT
public:
    explicit KeyMapEditor(QWidget *parent = nullptr);
    ~KeyMapEditor();

    void setPhoneScreenshot(const QPixmap &screenshot);
    void loadKeyMap(const QString &jsonFilePath);
    void saveKeyMap(const QString &jsonFilePath);

signals:
    void keyMapSaved(const QString &filePath);

private slots:
    void onAddKeyMap();
    void onDeleteKeyMap();
    void onKeyMapSelected(KeyMapWidget *widget);
    void onSaveKeyMap();
    void onLoadKeyMap();
    void onNewKeyMap();
    void onExportKeyMap();
    void onImportKeyMap();
    void onTypeChanged(int index);
    void onKeyBindingChanged();
    void onCommentChanged();
    void onPositionChanged();
    void onScreenshotCapture();

private:
    void setupUI();
    void setupToolbar();
    void setupPropertiesPanel();
    void setupScreenArea();
    void updatePropertiesPanel();
    void clearPropertiesPanel();
    void addKeyMapWidget(const QJsonObject &nodeData);
    QJsonObject createKeyMapJson();
    void loadFromJson(const QJsonObject &rootObj);

    // UI Components
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_contentLayout;

    // Toolbar
    QWidget *m_toolbar;
    QPushButton *m_btnNew;
    QPushButton *m_btnLoad;
    QPushButton *m_btnSave;
    QPushButton *m_btnExport;
    QPushButton *m_btnImport;
    QPushButton *m_btnCapture;
    QPushButton *m_btnAddKey;

    // Screen Area (shows phone screen with key mappings)
    QScrollArea *m_screenScrollArea;
    QLabel *m_screenLabel;
    QPixmap m_phoneScreenshot;
    QVector<KeyMapWidget*> m_keyMapWidgets;

    // Properties Panel (right side)
    QWidget *m_propertiesPanel;
    QVBoxLayout *m_propertiesLayout;
    QListWidget *m_keyMapList;

    // Property editors
    QComboBox *m_typeCombo;
    QLineEdit *m_keyBindingEdit;
    QLineEdit *m_commentEdit;
    QLineEdit *m_posXEdit;
    QLineEdit *m_posYEdit;
    QPushButton *m_btnDelete;

    // Additional property widgets for different types
    QWidget *m_additionalPropsWidget;
    QVBoxLayout *m_additionalPropsLayout;

    // Current selection
    KeyMapWidget *m_selectedWidget;

    // Switch key
    QString m_switchKey;
    QLineEdit *m_switchKeyEdit;

    // Mouse move settings
    QGroupBox *m_mouseMoveGroup;
    QLineEdit *m_mouseMoveStartX;
    QLineEdit *m_mouseMoveStartY;
    QLineEdit *m_mouseSpeedRatioX;
    QLineEdit *m_mouseSpeedRatioY;

    // Current file path
    QString m_currentFilePath;
};

#endif // KEYMAPEDITOR_H
