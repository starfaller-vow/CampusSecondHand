#include "MainWindow.h"
#include "LoginWindow.h"
#include "ClientConnector.h"
#include "../shared/Protocol.h"
#include "../shared/JsonConv.h"
#include "../shared/PasswordUtil.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QTabWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QTimer>
#include <QScrollArea>
#include <QColor>
#include <QFont>
#include <QPair>
#include <QHash>
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>
#include <functional>
#include <memory>

MainWindow::MainWindow(const User &user, QWidget *parent)
    : QMainWindow(parent)
    , m_user(user)//把传进来的 user 数据拷贝 / 赋给主窗口自己的成员，让主窗口在整个运行期间都 "记得" 这个用户是谁
{
    setupUI();
}

void MainWindow::setupUI()
{
    QString title = QStringLiteral("校园二手交易系统");
    if (m_user.isAdmin()) {
        title += QStringLiteral("  [管理员]");
    }
    title += QStringLiteral(" — ") + m_user.username;
    setWindowTitle(title);
    resize(1000, 680);

    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    // 消息刷新钩子（Tab5 填装；服务端推送新消息时由外部触发刷新）
    std::shared_ptr<std::function<void()>> msgRefreshHook;

    // ===== 懒加载机制：页面首次显示时才执行刷新（避免打开主窗口瞬间的请求风暴）=====
    auto registerLazy = [this](QWidget *page, std::function<void()> fn) {
        m_lazyRefreshers.insert(page, fn);
    };
    auto runLazy = [this](QWidget *page) {
        const auto it = m_lazyRefreshers.find(page);
        if (it == m_lazyRefreshers.end()) return;
        const std::function<void()> fn = it.value();
        m_lazyRefreshers.erase(it); // 先移除后执行，防止重入重复刷新
        fn();
    };

    // Tab 5: 消息
    qDebug() << "[setupUI] Tab5 消息 开始";
    m_tabMessages = new QWidget();
    {
        auto *l = new QVBoxLayout(m_tabMessages);

        auto *topBar = new QHBoxLayout();
        auto *unreadLabel = new QLabel();
        unreadLabel->setStyleSheet("font-size:14px;padding:4px 12px;border-radius:12px;background:#F44336;color:white;font-weight:bold;");
        auto *readAllBtn = new QPushButton(QStringLiteral("📖 全部标为已读"));
        auto *refreshBtn = new QPushButton(QStringLiteral("🔄 刷新"));
        topBar->addWidget(unreadLabel);
        topBar->addWidget(readAllBtn);
        topBar->addWidget(refreshBtn);
        topBar->addStretch();
        l->addLayout(topBar);

        auto *table = new QTableWidget(m_tabMessages);
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels(QStringList{
            QStringLiteral("类型"), QStringLiteral("标题"),
            QStringLiteral("时间"), QStringLiteral("状态")
        });
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        l->addWidget(table);

        // 缓存最近一次拉取的消息列表：双击查看正文时无需再次请求服务端
        auto msgsCache = std::make_shared<QList<Message>>();

        auto refreshMsg = [this, table, unreadLabel, msgsCache]() {
            table->setRowCount(0);

            // C/S：从服务端拉取消息
            QJsonObject data;
            QString err;
            if (!ClientConnector::instance().request(cs::cmd::MessageList, {}, data, err)) {
                QMessageBox::warning(this, QStringLiteral("失败"), err);
                return;
            }
            msgsCache->clear();
            for (const auto &v : data.value("messages").toArray()) {
                msgsCache->append(jsoncv::messageFromJson(v.toObject()));
            }
            QList<Message> msgs = *msgsCache;

            int unreadCount = 0;
            for (const Message &m : msgs) if (!m.read()) unreadCount++;

            unreadLabel->setText(unreadCount > 0
                ? QStringLiteral("🔴 未读 %1 条").arg(unreadCount)//数字由 arg 填充
                : QStringLiteral("✅ 全部已读"));
            unreadLabel->setStyleSheet(unreadCount > 0
                ? "font-size:14px;padding:4px 12px;border-radius:12px;background:#F44336;color:white;font-weight:bold;"//有未读：红色背景，白色粗字
                : "font-size:14px;padding:4px 12px;border-radius:12px;background:#4CAF50;color:white;font-weight:bold;");//全部已读：绿色背景，白色粗字

            // 未读优先排前，同是未读 / 同是已读时，新消息排在前面
            std::sort(msgs.begin(), msgs.end(), [](const Message &a, const Message &b) {
                if (a.isRead != b.isRead) return a.isRead < b.isRead;
                return a.createTime > b.createTime;
            });

            for (const Message &m : msgs) {//遍历消息列表里的每一条消息对象`m`
                int row = table->rowCount();//拿到已有行数
                table->insertRow(row);
                QString typeIcon;
                if (m.type == "trade")    typeIcon = QStringLiteral("💼");
                else if (m.type == "report") typeIcon = QStringLiteral("🚩");//举报消息
                else if (m.type == "feedback") typeIcon = QStringLiteral("📮");//用户反馈（管理员收到）
                else                      typeIcon = QStringLiteral("📢");//系统消息

                table->setItem(row, 0, new QTableWidgetItem(typeIcon));//第 0 列填入消息图标

                auto *titleItem = new QTableWidgetItem(m.title);
                titleItem->setData(Qt::UserRole, m.id);//把消息 id 藏进单元格，双击时按 id 精确查找
                if (!m.read()) {
                    titleItem->setFont(QFont("", -1, QFont::Bold));
                    titleItem->setForeground(QColor("#1976D2"));
                }
                table->setItem(row, 1, titleItem);
                table->setItem(row, 2, new QTableWidgetItem(m.createTime));
                table->setItem(row, 3, new QTableWidgetItem(
                    m.read() ? QStringLiteral("✅ 已读") : QStringLiteral("🔴 未读")));
            }
        };
        // 填装刷新钩子：服务端推送 message.new 时外部触发
        msgRefreshHook = std::make_shared<std::function<void()>>(refreshMsg);

        // 推送防抖：短时间内多条推送（如一次购买产生买卖双方两条消息）只刷新一次，
        // 且刷新被推迟到当前网络回调结束后执行，避免在请求等待中嵌套发请求
        if (!m_msgDebounceTimer) {
            m_msgDebounceTimer = new QTimer(this);
            m_msgDebounceTimer->setSingleShot(true);
            connect(m_msgDebounceTimer, &QTimer::timeout, this, [this, refreshMsg]() {
                refreshMsg();
                // 交易结算/退款等余额变动都伴随消息推送，顺带刷新余额流水，
                // 使卖家到账收入无需手动点刷新即可显示
                if (m_balanceRefreshHook) m_balanceRefreshHook();
            });
        }

        // 双击查看详情并标记已读
        connect(table, &QTableWidget::cellDoubleClicked, this, [this, table, refreshMsg, msgsCache](int row, int) {//绑定信号：**双击表格单元格**，执行 lambda。
            auto *titleItem = table->item(row, 1);
            if (!titleItem) return;
            int msgId = titleItem->data(Qt::UserRole).toInt();
            for (Message &m : *msgsCache) {
                if (m.id != msgId) continue;
                if (!m.read()) {
                    // C/S：通知服务端标记已读，本地缓存同步更新
                    QJsonObject rdata;
                    QString err;
                    if (ClientConnector::instance().request(cs::cmd::MessageMarkRead,
                            QJsonObject{ {"msgId", msgId} }, rdata, err)) {
                        m.isRead = 1;
                    } else {
                        QMessageBox::warning(this, QStringLiteral("失败"), err);
                        break;
                    }
                }
                // 展示消息正文（表格里只有标题，正文靠双击弹窗查看）
                QMessageBox::information(this, m.title, m.content.isEmpty() ? QStringLiteral("（无正文）") : m.content);
                break;
            }
            refreshMsg();
        });

        connect(readAllBtn, &QPushButton::clicked, this, [this, refreshMsg]() {//全部标为已读
            QJsonObject rdata;
            QString err;
            if (!ClientConnector::instance().request(cs::cmd::MessageReadAll, {}, rdata, err)) {
                QMessageBox::warning(this, QStringLiteral("失败"), err);
                return;
            }
            refreshMsg();//刷新页面
        });
        connect(refreshBtn, &QPushButton::clicked, refreshMsg);

        registerLazy(m_tabMessages, refreshMsg); // 首次显示消息页时才拉取
    }
    m_tabWidget->addTab(m_tabMessages, QStringLiteral("📨 我的消息"));
    qDebug() << "[setupUI] Tab5 消息 完成";

    // Tab 6: 个人中心
    qDebug() << "[setupUI] Tab6 个人中心 开始";
    m_tabProfile = new QWidget();
    {
        auto *mainL = new QHBoxLayout(m_tabProfile);

        // 左半边：基本信息 + 操作按钮
        auto *leftPanel = new QWidget();
        auto *leftL = new QVBoxLayout(leftPanel);

        auto *infoBox = new QLabel();
        infoBox->setStyleSheet(
            "QLabel { background:#FAFAFA; padding:20px; border-radius:8px; "
            "font-size:14px; line-height:1.8px; }"
        );
        auto refreshInfo = [this, infoBox]() {
            // C/S：从服务端拉取自己的最新信息
            QJsonObject data;
            QString err;
            if (!ClientConnector::instance().request(cs::cmd::Self, {}, data, err)) {
                QMessageBox::warning(this, QStringLiteral("失败"), err);
                return;
            }
            User fresh = jsoncv::userFromJson(data.value("user").toObject());
            m_user = fresh;//用服务端最新用户信息覆盖内存里的`m_user`变量，保证数据是最新的。
            QString roleText = fresh.isAdmin() ? QStringLiteral("🛡️ 管理员") : QStringLiteral("👤 普通用户");//身份
            infoBox->setText(QStringLiteral(
                "<h3 style='margin:0 0 10px 0;'>%1</h3>"//用户名
                "<p><b>学号：</b>%2</p>"
                "<p><b>身份：</b>%3</p>"
                "<p><b>学院：</b>%4</p>"
                "<p><b>联系方式：</b>%5</p>"
                "<p><b>注册时间：</b>%6</p>"
                "<p style='margin-top:12px;font-size:18px;color:#2E7D32;'>💰 余额：¥%7</p>"
            ).arg(fresh.username, fresh.studentNo, roleText,
                  fresh.college.isEmpty() ? QStringLiteral("—") : fresh.college,
                  fresh.contact.isEmpty() ? QStringLiteral("未设置") : fresh.contact,
                  fresh.createTime,
                  QStringLiteral("¥%1").arg(fresh.balance, 0, 'f', 2)));
        };
        leftL->addWidget(infoBox);

        // 按钮组
        auto *btnGrid = new QGridLayout();
        auto makeBtn = [](const QString &text, const QString &color) {//`text`按钮文字，`color`背景色
            auto *b = new QPushButton(text);
            b->setMinimumHeight(44);
            b->setStyleSheet(QString(
                "QPushButton { background:%1; color:white; padding:8px 16px; border-radius:6px; font-size:13px; }"
                "QPushButton:hover { filter: brightness(0.9); }"
            ).arg(color));
            return b;
        };

        auto *contactBtn = makeBtn(QStringLiteral("📝 修改联系方式"), "#2196F3");
        auto *pwdBtn     = makeBtn(QStringLiteral("🔑 修改密码"),    "#FF9800");
        btnGrid->addWidget(contactBtn, 0, 0);
        btnGrid->addWidget(pwdBtn,     0, 1);
        leftL->addLayout(btnGrid);
        leftL->addStretch();

        mainL->addWidget(leftPanel, 1);

        // ===== 修改联系方式 =====
        connect(contactBtn, &QPushButton::clicked, this, [this, refreshInfo]() {
            bool ok;
            QString contact = QInputDialog::getText(this, QStringLiteral("修改联系方式"),
                QStringLiteral("请输入新的联系方式："), QLineEdit::Normal,
                m_user.contact, &ok);
            if (!ok) return;
            QJsonObject rdata;
            QString err;
            if (ClientConnector::instance().request(cs::cmd::UpdateContact,
                    QJsonObject{ {"contact", contact} }, rdata, err)) {
                QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("联系方式已更新"));
                refreshInfo();
            } else {
                QMessageBox::warning(this, QStringLiteral("失败"), err);
            }
        });

        // ===== 修改密码 =====
        connect(pwdBtn, &QPushButton::clicked, this, [this, refreshInfo]() {
            bool ok1, ok2, ok3;
            QString oldPwd = QInputDialog::getText(this, QStringLiteral("修改密码"),
                QStringLiteral("请输入旧密码："), QLineEdit::Password, "", &ok1);
            if (!ok1) return;//// 用户点取消，直接退出
            QString newPwd = QInputDialog::getText(this, QStringLiteral("修改密码"),
                QStringLiteral("请输入新密码（≥8位）："), QLineEdit::Password, "", &ok2);
            if (!ok2) return;
            QString newPwd2 = QInputDialog::getText(this, QStringLiteral("修改密码"),
                QStringLiteral("请再次输入新密码："), QLineEdit::Password, "", &ok3);
            if (!ok3) return;
            if (newPwd != newPwd2) {
                QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("两次输入的新密码不一致"));
                return;
            }
            // 客户端本地校验新密码强度（与服务端注册规则一致）
            if (newPwd.size() < 8) {
                QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("密码至少8位"));
                return;
            }
            if (!newPwd.contains(QRegularExpression("[a-zA-Z]"))
                || !newPwd.contains(QRegularExpression("[0-9]"))) {
                QMessageBox::warning(this, QStringLiteral("失败"), QStringLiteral("密码必须同时包含字母和数字"));
                return;
            }
            // C/S：新旧密码都在客户端先哈希，服务端直接比对哈希值
            QJsonObject rdata;
            QString err;
            if (ClientConnector::instance().request(cs::cmd::ChangePassword, QJsonObject{
                    {"oldPasswordHash", passutil::sha256Hex(oldPwd)},
                    {"newPasswordHash", passutil::sha256Hex(newPwd)}
                }, rdata, err)) {
                QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("密码修改成功"));
            } else {
                QMessageBox::warning(this, QStringLiteral("失败"), err);
            }
        });

        // ===== 联系管理员（反馈消息直达管理员消息箱，仅普通用户可见）=====
        if (!m_user.isAdmin()) {
            auto *contactAdminBtn = makeBtn(QStringLiteral("📮 联系管理员"), "#00897B");
            btnGrid->addWidget(contactAdminBtn, 2, 0, 1, 2);
            connect(contactAdminBtn, &QPushButton::clicked, this, [this]() {
                bool ok;
                QString content = QInputDialog::getMultiLineText(this, QStringLiteral("联系管理员"),
                    QStringLiteral("请输入你要反馈的内容："), QString(), &ok).trimmed();
                if (!ok) return;
                if (content.isEmpty()) {
                    QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("内容不能为空"));
                    return;
                }
                if (content.length() > 500) {
                    QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("内容过长（最多500字）"));
                    return;
                }
                // C/S：服务端负责把反馈插入全部管理员的消息箱
                QJsonObject data;
                QString err;
                if (!ClientConnector::instance().request(cs::cmd::ContactAdmin,
                        QJsonObject{ {"content", content} }, data, err)) {
                    QMessageBox::warning(this, QStringLiteral("失败"), err);
                    return;
                }
                if (data.value("sent").toInt() == 0) {
                    QMessageBox::warning(this, QStringLiteral("失败"),
                        QStringLiteral("系统暂无管理员，无法发送"));
                    return;
                }
                QMessageBox::information(this, QStringLiteral("已发送"),
                    QStringLiteral("反馈已发送给管理员，请耐心等待处理。"));
            });
        }
        registerLazy(m_tabProfile, [refreshInfo]() {
            refreshInfo();
        });
    }
    m_tabWidget->addTab(m_tabProfile, QStringLiteral("👤 个人中心"));
    qDebug() << "[setupUI] Tab6 个人中心 完成";

    // Tab 7: 管理员面板（仅管理员）
    if (m_user.isAdmin()) {
        qDebug() << "[setupUI] Tab7 管理员面板 开始";
        m_tabAdmin = new QWidget();
        auto *adminL = new QVBoxLayout(m_tabAdmin);
        auto *adminTabs = new QTabWidget(m_tabAdmin);
        adminL->addWidget(adminTabs);

        // ===== 子Tab 2: 分类管理 =====
        {
            auto *page = new QWidget();
            auto *l = new QVBoxLayout(page);
            auto *btnBar = new QHBoxLayout();
            auto *addBtn = new QPushButton(QStringLiteral("➕ 新增分类"));
            auto *refreshBtn = new QPushButton(QStringLiteral("🔄 刷新"));
            btnBar->addWidget(addBtn);
            btnBar->addWidget(refreshBtn);
            btnBar->addStretch();
            l->addLayout(btnBar);

            auto *table = new QTableWidget(page);
            table->setColumnCount(4);
            table->setHorizontalHeaderLabels(QStringList{
                QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("排序"), QStringLiteral("操作")
            });
            table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            l->addWidget(table);

            auto refreshCats = std::make_shared<std::function<void()>>();
            *refreshCats = [this, table, refreshCats]() {
                table->setRowCount(0);

                // C/S：从服务端拉取全部分类
                QJsonObject data;
                QString err;
                if (!ClientConnector::instance().request(cs::cmd::ListCategories, {}, data, err)) {
                    QMessageBox::warning(this, QStringLiteral("失败"), err);
                    return;
                }
                QList<Category> cats;
                for (const auto &v : data.value("categories").toArray()) {
                    cats.append(jsoncv::categoryFromJson(v.toObject()));
                }

                for (const auto &c : cats) {
                    int row = table->rowCount();
                    table->insertRow(row);
                    table->setItem(row, 0, new QTableWidgetItem(QString::number(c.id)));
                    table->setItem(row, 1, new QTableWidgetItem(c.name));
                    table->setItem(row, 2, new QTableWidgetItem(QString::number(c.sortOrder)));// 第2列：排序号sortOrder，用于控制分类展示先后顺序
                    // ========== 第3列【操作列】准备容器，放编辑、删除按钮 ==========
                    auto *bw = new QWidget();
                    auto *bl = new QHBoxLayout(bw);
                    bl->setContentsMargins(2, 2, 2, 2);
                    bl->setSpacing(4);

                    auto *editBtn = new QPushButton(QStringLiteral("编辑"));
                    editBtn->setStyleSheet("background:#2196F3;color:white;padding:2px 8px;border-radius:3px;");// 设置按钮样式：蓝色背景、白字、内边距、圆角
                    connect(editBtn, &QPushButton::clicked, this, [this, c, refreshCats]() {
                        bool ok;
                        QString name = QInputDialog::getText(this, QStringLiteral("编辑分类"),
                            QStringLiteral("分类名称："), QLineEdit::Normal, c.name, &ok);
                        if (!ok || name.isEmpty()) return;// 如果用户点取消，或者输入的名称为空，直接退出，不执行更新
                        int order = QInputDialog::getInt(this, QStringLiteral("编辑分类"),
                            QStringLiteral("排序："), c.sortOrder, 0, 9999, 1, &ok);// 弹出数字输入框，修改排序号，默认值c.sortOrder；范围0~9999，步长1
                        if (!ok) return;
                        QJsonObject rdata;
                        QString err;
                        if (ClientConnector::instance().request(cs::cmd::CategoryUpdate, QJsonObject{
                                {"id", c.id}, {"name", name}, {"sortOrder", order}
                            }, rdata, err)) {
                            (*refreshCats)();
                        } else {
                            QMessageBox::warning(this, QStringLiteral("失败"), err);
                        }
                    });
                    bl->addWidget(editBtn);

                    auto *delBtn = new QPushButton(QStringLiteral("删除"));
                    delBtn->setStyleSheet("background:#F44336;color:white;padding:2px 8px;border-radius:3px;");
                    connect(delBtn, &QPushButton::clicked, this, [this, c, refreshCats]() {
                        auto ret = QMessageBox::warning(this, QStringLiteral("确认"),
                            QStringLiteral("确定删除分类「%1」？").arg(c.name),
                            QMessageBox::Yes | QMessageBox::No);
                        if (ret != QMessageBox::Yes) return;// 如果用户不是点Yes（点No或者关闭弹窗），直接退出，不执行删除
                        QJsonObject rdata;
                        QString err;
                        if (ClientConnector::instance().request(cs::cmd::CategoryRemove,
                                QJsonObject{ {"id", c.id} }, rdata, err)) {
                            (*refreshCats)();
                        } else {
                            QMessageBox::warning(this, QStringLiteral("失败"), err);
                        }
                    });
                    bl->addWidget(delBtn);

                    table->setCellWidget(row, 3, bw);
                }
            };
            // 绑定【新增分类】按钮点击事件
            connect(addBtn, &QPushButton::clicked, this, [this, refreshCats]() {
                bool ok;
                QString name = QInputDialog::getText(this, QStringLiteral("新增分类"),
                    QStringLiteral("分类名称："), QLineEdit::Normal, QString(), &ok);
                if (!ok || name.isEmpty()) return;
                QJsonObject rdata;
                QString err;
                if (ClientConnector::instance().request(cs::cmd::CategoryAdd,
                        QJsonObject{ {"name", name}, {"sortOrder", 0} }, rdata, err)) {
                    (*refreshCats)();// 调用服务端插入这条新分类
                } else {
                    QMessageBox::warning(this, QStringLiteral("失败"), err);
                }
            });
            connect(refreshBtn, &QPushButton::clicked, [refreshCats]() { (*refreshCats)(); });
            registerLazy(page, *refreshCats);
            adminTabs->addTab(page, QStringLiteral("📂 分类管理"));
        }

        // ===== 子Tab 3: 手续费 + 推荐系数调节 =====
        {
            auto *page = new QWidget();
            auto *l = new QVBoxLayout(page);

            auto *feeGroup = new QGroupBox(QStringLiteral("💸 手续费规则"), page);
            auto *feeL = new QFormLayout(feeGroup);
            auto *thresholdSpin = new QDoubleSpinBox(feeGroup);// 起征额输入框（小数旋转框）
            thresholdSpin->setRange(0, 100000); thresholdSpin->setSuffix(QStringLiteral(" 元"));
            auto *rateSpin = new QDoubleSpinBox(feeGroup);// 手续费率输入框
            // 0 ~ 1，对应0% ~100%,点一次上下箭头变动0.001,保留3位小数，支持0.5%这种费率
            rateSpin->setRange(0, 1); rateSpin->setSingleStep(0.001); rateSpin->setDecimals(3);
            auto *saveFeeBtn = new QPushButton(QStringLiteral("💾 保存手续费设置"));
            auto *timeoutSpin = new QSpinBox(feeGroup);// 超时自动结算天数（担保交易闭环）
            timeoutSpin->setRange(1, 365); timeoutSpin->setSuffix(QStringLiteral(" 天"));
            feeL->addRow(QStringLiteral("起征额（满多少元才开始收手续费）："), thresholdSpin);
            feeL->addRow(QStringLiteral("费率（超过起征额部分的比例，如 0.005 表示 0.5%）："), rateSpin);
            feeL->addRow(QStringLiteral("自动结算天数（付款后超该天数未确认收货，货款自动结算给卖家）："), timeoutSpin);
            feeL->addRow(saveFeeBtn);

            auto *coefGroup = new QGroupBox(QStringLiteral("📉 磨损推荐系数"), page);
            auto *coefL = new QFormLayout(coefGroup);
            QList<QPair<QString, QDoubleSpinBox*>> coefSpins;
            QStringList conditions{
                QStringLiteral("全新"), QStringLiteral("九成新"), QStringLiteral("八成新"),
                QStringLiteral("七成新"), QStringLiteral("六成新及以下")
            };
            // 循环为每一种成色生成一个小数输入框
            for (const auto &cond : conditions) {
                auto *spin = new QDoubleSpinBox(coefGroup);
                spin->setRange(0, 1); spin->setSingleStep(0.05); spin->setDecimals(2);
                coefL->addRow(QStringLiteral("%1：").arg(cond), spin);
                coefSpins.append({cond, spin});// 存入列表，方便后面读取、赋值
            }
            auto *saveCoefBtn = new QPushButton(QStringLiteral("💾 保存推荐系数"));
            coefL->addRow(saveCoefBtn);

            l->addWidget(feeGroup);
            l->addWidget(coefGroup);
            l->addStretch();

            // C/S：首次显示该页时从服务端读取当前设置填充输入框
            registerLazy(page, [this, thresholdSpin, rateSpin, timeoutSpin, coefSpins]() {
                QJsonObject sdata;
                QString serr;
                if (ClientConnector::instance().request(cs::cmd::SettingsGet, {}, sdata, serr)) {
                    thresholdSpin->setValue(sdata.value("feeThreshold").toDouble());
                    rateSpin->setValue(sdata.value("feeRate").toDouble());
                    timeoutSpin->setValue(sdata.value("timeoutDays").toInt());
                    const QJsonObject coef = sdata.value("coef").toObject();
                    for (const auto &p : coefSpins) {
                        p.second->setValue(coef.value(p.first).toDouble());
                    }
                } else {
                    QMessageBox::warning(this, QStringLiteral("失败"), serr);
                }
            });

            connect(saveFeeBtn, &QPushButton::clicked, this, [thresholdSpin, rateSpin, timeoutSpin]() {
                QJsonObject rdata;
                QString err;
                if (ClientConnector::instance().request(cs::cmd::SettingsSet, QJsonObject{
                        {"feeThreshold", thresholdSpin->value()},
                        {"feeRate", rateSpin->value()},
                        {"timeoutDays", timeoutSpin->value()}
                    }, rdata, err)) {
                    QMessageBox::information(nullptr, QStringLiteral("成功"), QStringLiteral("手续费设置已保存"));
                } else {
                    QMessageBox::warning(nullptr, QStringLiteral("失败"), err);
                }
            });
            connect(saveCoefBtn, &QPushButton::clicked, this, [coefSpins]() {
                QJsonObject coefObj;
                for (const auto &p : coefSpins) {
                    coefObj.insert(p.first, p.second->value());
                }
                QJsonObject rdata;
                QString err;
                if (ClientConnector::instance().request(cs::cmd::SettingsSet,
                        QJsonObject{ {"coef", coefObj} }, rdata, err)) {
                    QMessageBox::information(nullptr, QStringLiteral("成功"), QStringLiteral("推荐系数已保存"));
                } else {
                    QMessageBox::warning(nullptr, QStringLiteral("失败"), err);
                }
            });

            adminTabs->addTab(page, QStringLiteral("💸 手续费 / 推荐系数"));
        }

        // ===== 子Tab 4: 充值码批量生成 =====
        {
            auto *page = new QWidget();
            auto *l = new QVBoxLayout(page);
            auto *form = new QGridLayout();
            auto *amountSpin = new QDoubleSpinBox(page);
            amountSpin->setRange(1, 100000); amountSpin->setSuffix(QStringLiteral(" 元"));// 金额范围：1 ~ 100000
            auto *countSpin = new QSpinBox(page);
            countSpin->setRange(1, 100);// 一次最多生成100个充值码
            auto *genBtn = new QPushButton(QStringLiteral("🎁 生成充值码"));
            genBtn->setStyleSheet("background:#9C27B0;color:white;padding:8px 20px;border-radius:6px;");
            form->addWidget(new QLabel(QStringLiteral("金额：")), 0, 0);
            form->addWidget(amountSpin, 0, 1);
            form->addWidget(new QLabel(QStringLiteral("数量：")), 1, 0);
            form->addWidget(countSpin, 1, 1);
            form->addWidget(genBtn, 2, 0, 1, 2);
            l->addLayout(form);

            auto *resultEdit = new QTextEdit(page);// 文本框，展示生成好的充值码
            resultEdit->setReadOnly(true);// 设置只读，管理员只能看不能修改
            resultEdit->setPlaceholderText(QStringLiteral("生成的充值码会显示在这里..."));
            l->addWidget(new QLabel(QStringLiteral("📋 生成结果：")));
            l->addWidget(resultEdit);

            connect(genBtn, &QPushButton::clicked, this, [amountSpin, countSpin, resultEdit]() {
                QJsonObject data;
                QString err;
                if (!ClientConnector::instance().request(cs::cmd::RedeemGenerate, QJsonObject{
                        {"amount", amountSpin->value()}, {"count", countSpin->value()}
                    }, data, err)) {
                    QMessageBox::warning(nullptr, QStringLiteral("失败"), err);
                    return;
                }
                QStringList codes;
                for (const auto &v : data.value("codes").toArray()) {
                    codes.append(v.toString());
                }
                resultEdit->setText(codes.join("\n"));
            });

            adminTabs->addTab(page, QStringLiteral("🎁 充值码生成"));
        }

        // 管理员面板懒加载：首次显示触发默认子页；切换子页时各自首次刷新
        registerLazy(m_tabAdmin, [adminTabs, runLazy]() { runLazy(adminTabs->currentWidget()); });
        connect(adminTabs, &QTabWidget::currentChanged, this,
                [adminTabs, runLazy]() { runLazy(adminTabs->currentWidget()); });

        m_tabWidget->addTab(m_tabAdmin, QStringLiteral("🛡️ 管理员面板"));
        qDebug() << "[setupUI] Tab7 管理员面板 完成";
    }

    // 主 Tab 切换：首次显示某页时才拉数据（懒加载；登录后只加载默认的浏览页）
    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this, runLazy]() {
        runLazy(m_tabWidget->currentWidget());
    });
    runLazy(m_tabWidget->currentWidget()); // 默认页（商品浏览）立即首次加载

    // 状态栏
    statusBar()->showMessage(
        QStringLiteral("欢迎，%1 | 学号：%2 | 余额：¥%3")
            .arg(m_user.username).arg(m_user.studentNo).arg(m_user.balance)
    );

    // C/S：订阅服务端推送与断线事件
    // 回登录（踢下线/断线共用，标志位防重复弹窗）
    auto backToLogin = [this](const QString &title, const QString &text) {
        if (m_goingBackToLogin) return;
        m_goingBackToLogin = true;
        QMessageBox::critical(this, title, text);
        ClientConnector::instance().disconnectFromServer();
        auto *login = new LoginWindow();
        login->setAttribute(Qt::WA_DeleteOnClose);
        login->show();
        deleteLater();
    };

    connect(&ClientConnector::instance(), &ClientConnector::pushed, this,
            [this, backToLogin](const QString &type, const QJsonObject &data) {
        if (type == cs::push::Message) {
            // 200ms 防抖合并，避免推送风暴触发请求风暴
            if (m_msgDebounceTimer) m_msgDebounceTimer->start(200);
            statusBar()->showMessage(QStringLiteral("📨 收到新消息，请查看「我的消息」"), 3000);
        }
    });

    // 断线（服务端重启/网络异常）→ 提示并退回登录窗
    connect(&ClientConnector::instance(), &ClientConnector::disconnectedFromServer, this,
            [this, backToLogin]() {
        backToLogin(QStringLiteral("连接已断开"),
            QStringLiteral("与服务器的连接已断开（可能服务端已重启）。\n请重新登录。"));
    });
}