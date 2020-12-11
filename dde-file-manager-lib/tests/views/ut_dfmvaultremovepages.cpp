#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <QTest>
#include <QPlainTextEdit>
#include <QPushButton>
#include <DToolTip>
#include "views/dfmvaultremoveprogressview.h"
#include "vault/vaultlockmanager.h"

#include <QStackedWidget>

#include "stub.h"

#define private public
#include "views/dfmvaultremovepages.h"

DWIDGET_USE_NAMESPACE
namespace  {
    class TestDFMVaultRemovePages: public testing::Test
    {
    public:
        QSharedPointer<DFMVaultRemovePages> m_view;

        virtual void SetUp() override
        {
            m_view = QSharedPointer<DFMVaultRemovePages>(new DFMVaultRemovePages());
            m_view->show();
            std::cout << "start TestDFMVaultRemovePages" << std::endl;
        }

        virtual void TearDown() override
        {
            m_view->close();
            std::cout << "end TestDFMVaultRemovePages" << std::endl;
        }
    };
}


TEST_F(TestDFMVaultRemovePages, tst_getPassword)
{
    m_view->onButtonClicked(0);
    m_view->onButtonClicked(1);
    m_view->onButtonClicked(2);
    m_view->onButtonClicked(3);
}

TEST_F(TestDFMVaultRemovePages, tst_onLockVault)
{
    m_view->m_bRemoveVault = true;

    void (*st_removeVault)(const QString &, const QString &) = [](const QString &, const QString &){
        // do nothing.
    };

    Stub stub;
    stub.set(&DFMVaultRemoveProgressView::removeVault, st_removeVault);

    m_view->onLockVault(0);
}

TEST_F(TestDFMVaultRemovePages, tst_onVualtRemoveFinish)
{
    m_view->onVualtRemoveFinish(true);
    m_view->onVualtRemoveFinish(false);
    EXPECT_TRUE(m_view->getButton(0)->isEnabled());
}

TEST_F(TestDFMVaultRemovePages, tst_showRemoveWidget)
{
    m_view->showRemoveWidget();
}

TEST_F(TestDFMVaultRemovePages, tst_onButtonClicked)
{
    m_view->onButtonClicked(0);

    m_view->m_stackedWidget->setCurrentIndex(0);
    m_view->onButtonClicked(1);
    EXPECT_EQ(1, m_view->m_stackedWidget->currentIndex());

    m_view->m_stackedWidget->setCurrentIndex(1);
    m_view->onButtonClicked(1);
    EXPECT_EQ(static_cast<int>(0), m_view->m_stackedWidget->currentIndex());

    // replace vaultlockmanager::checkauthentication
    bool (*st_checkAuthentication)(QString) = [](QString)->bool {
        return true;
    };

    Stub stub;
    stub.set(ADDR(VaultLockManager, checkAuthentication), st_checkAuthentication);

    m_view->m_stackedWidget->setCurrentIndex(0);
    m_view->onButtonClicked(2);

    m_view->m_stackedWidget->setCurrentIndex(1);
    m_view->onButtonClicked(2);
}

TEST_F(TestDFMVaultRemovePages, tst_onRemoveFinish)
{
    m_view->onVualtRemoveFinish(true);
    EXPECT_FALSE(m_view->m_pInfo->text().isEmpty());

    m_view->onVualtRemoveFinish(false);
    EXPECT_FALSE(m_view->m_pInfo->text().isEmpty());
}