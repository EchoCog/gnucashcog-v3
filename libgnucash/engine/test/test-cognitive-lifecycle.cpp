/********************************************************************\
 * test-cognitive-lifecycle.cpp -- Book mapping, KVP, hooks, snapshot *
 * Copyright (C) 2024-2026 GnuCash Cognitive Engine                   *
\********************************************************************/

#include <glib.h>
#include <glib/gstdio.h>
#include <gtest/gtest.h>
#include "gnc-cognitive-accounting.h"
#include "gnc-fincosys-bridge.h"
#include "Account.h"
#include "Transaction.h"
#include "Split.h"
#include "qof.h"
#include "gnc-engine.h"
#include "gnc-commodity.h"

class CognitiveLifecycleTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        qof_init();
        ASSERT_TRUE(gnc_cognitive_accounting_init());

        book = qof_book_new();
        auto *commodity_table = gnc_commodity_table_get_table(book);
        default_currency = gnc_commodity_table_lookup(
            commodity_table, GNC_COMMODITY_NS_CURRENCY, "USD");
        if (!default_currency)
        {
            default_currency = gnc_commodity_new(
                book, "US Dollar", GNC_COMMODITY_NS_CURRENCY, "USD", "840", 100);
            gnc_commodity_table_insert(commodity_table, default_currency);
        }

        root_account = gnc_account_create_root(book);

        checking_account = xaccMallocAccount(book);
        xaccAccountBeginEdit(checking_account);
        xaccAccountSetName(checking_account, "Checking");
        xaccAccountSetType(checking_account, ACCT_TYPE_BANK);
        xaccAccountSetCommodity(checking_account, default_currency);
        xaccAccountCommitEdit(checking_account);
        gnc_account_append_child(root_account, checking_account);

        expense_account = xaccMallocAccount(book);
        xaccAccountBeginEdit(expense_account);
        xaccAccountSetName(expense_account, "Groceries");
        xaccAccountSetType(expense_account, ACCT_TYPE_EXPENSE);
        xaccAccountSetCommodity(expense_account, default_currency);
        xaccAccountCommitEdit(expense_account);
        gnc_account_append_child(root_account, expense_account);
    }

    void TearDown() override
    {
        gnc_cognitive_set_enabled(FALSE);
        if (gnc_cognitive_accounting_is_initialized())
            gnc_cognitive_accounting_shutdown();
        qof_book_destroy(book);
        qof_close();
    }

    Transaction *create_balanced_transaction(gnc_numeric amount)
    {
        Transaction *tx = xaccMallocTransaction(book);
        xaccTransBeginEdit(tx);
        xaccTransSetCurrency(tx, default_currency);
        xaccTransSetDescription(tx, "lifecycle test");

        Split *s1 = xaccMallocSplit(book);
        xaccSplitSetAccount(s1, checking_account);
        xaccSplitSetParent(s1, tx);
        xaccSplitSetValue(s1, gnc_numeric_neg(amount));
        xaccSplitSetAmount(s1, gnc_numeric_neg(amount));

        Split *s2 = xaccMallocSplit(book);
        xaccSplitSetAccount(s2, expense_account);
        xaccSplitSetParent(s2, tx);
        xaccSplitSetValue(s2, amount);
        xaccSplitSetAmount(s2, amount);

        xaccTransCommitEdit(tx);
        return tx;
    }

    QofBook *book = nullptr;
    Account *root_account = nullptr;
    Account *checking_account = nullptr;
    Account *expense_account = nullptr;
    gnc_commodity *default_currency = nullptr;
};

TEST_F(CognitiveLifecycleTest, IsInitializedAndCapabilityReport)
{
    EXPECT_TRUE(gnc_cognitive_accounting_is_initialized());
    gchar *report = gnc_cognitive_capability_report();
    ASSERT_NE(report, nullptr);
    EXPECT_NE(strstr(report, "atomspace_backend"), nullptr);
    EXPECT_NE(strstr(report, "simulated"), nullptr);
    g_free(report);

    gchar *json = gnc_cognitive_dump_state_json();
    ASSERT_NE(json, nullptr);
    EXPECT_NE(strstr(json, "gnucashcog-cognitive-state/v1"), nullptr);
    g_free(json);
}

TEST_F(CognitiveLifecycleTest, BookToAtomSpaceMapsAccounts)
{
    guint before = gnc_atomspace_count_atoms();
    gint mapped = gnc_book_to_atomspace(book);
    EXPECT_GE(mapped, 2); /* at least checking + expense (+ root) */
    EXPECT_GT(gnc_atomspace_count_atoms(), before);

    GncAtomHandle h = gnc_account_to_atomspace(checking_account);
    EXPECT_NE(h, 0u);
}

TEST_F(CognitiveLifecycleTest, CognitiveTypeKvpRoundTrip)
{
    EXPECT_EQ(gnc_account_get_cognitive_type(checking_account),
              GNC_COGNITIVE_ACCT_TRADITIONAL);

    gnc_account_set_cognitive_type(checking_account, GNC_COGNITIVE_ACCT_ADAPTIVE);
    EXPECT_EQ(gnc_account_get_cognitive_type(checking_account),
              GNC_COGNITIVE_ACCT_ADAPTIVE);

    gnc_account_set_cognitive_type(checking_account, GNC_COGNITIVE_ACCT_ATTENTION);
    EXPECT_EQ(gnc_account_get_cognitive_type(checking_account),
              GNC_COGNITIVE_ACCT_ATTENTION);
}

TEST_F(CognitiveLifecycleTest, TransactionCommitUpdatesAttention)
{
    gnc_book_to_atomspace(book);

    GncAttentionParams before =
        gnc_ecan_get_attention_params(checking_account);

    create_balanced_transaction(gnc_numeric_create(2500, 100));

    GncAttentionParams after =
        gnc_ecan_get_attention_params(checking_account);

    /* Commit hook should have run ECAN update; STI or activity should move. */
    EXPECT_TRUE(after.sti != before.sti ||
                after.activity_level != before.activity_level ||
                after.lti != before.lti);
}

TEST_F(CognitiveLifecycleTest, SnapshotRoundTrip)
{
    gnc_book_to_atomspace(book);
    guint count = gnc_atomspace_count_atoms();
    ASSERT_GT(count, 0u);

    gchar *path = g_build_filename(g_get_tmp_dir(),
                                   "gnucashcog-lifecycle-snapshot.json",
                                   nullptr);
    ASSERT_TRUE(gnc_cognitive_save_snapshot(path));

    gnc_cognitive_accounting_shutdown();
    ASSERT_FALSE(gnc_cognitive_accounting_is_initialized());

    gint loaded = gnc_cognitive_load_snapshot(path);
    EXPECT_GE(loaded, 1);
    EXPECT_TRUE(gnc_cognitive_accounting_is_initialized());
    EXPECT_GT(gnc_atomspace_count_atoms(), 0u);

    g_unlink(path);
    g_free(path);
}

TEST_F(CognitiveLifecycleTest, LinkOutgoingStoredForInheritance)
{
    GncAtomHandle child = gnc_atomspace_create_concept_node("ChildAcct");
    GncAtomHandle parent = gnc_atomspace_create_concept_node("ParentAcct");
    GncAtomHandle link = gnc_atomspace_create_inheritance_link(child, parent);
    ASSERT_NE(link, 0u);

    gsize n = 0;
    ASSERT_TRUE(gnc_atomspace_get_outgoing(link, nullptr, &n));
    EXPECT_EQ(n, 2u);

    GncAtomHandle outs[4] = {0};
    gsize cap = 4;
    ASSERT_TRUE(gnc_atomspace_get_outgoing(link, outs, &cap));
    EXPECT_EQ(cap, 2u);
    EXPECT_EQ(outs[0], child);
    EXPECT_EQ(outs[1], parent);
}

TEST_F(CognitiveLifecycleTest, FeatureFlagEnv)
{
    g_unsetenv("GNC_COGNITIVE_ENABLED");
    gnc_cognitive_set_enabled(FALSE);
    EXPECT_FALSE(gnc_cognitive_is_enabled());

    gnc_cognitive_set_enabled(TRUE);
    EXPECT_TRUE(gnc_cognitive_is_enabled());

    gnc_cognitive_set_enabled(FALSE);
    g_setenv("GNC_COGNITIVE_ENABLED", "1", TRUE);
    /* explicit disable wins over env until re-enabled */
    EXPECT_FALSE(gnc_cognitive_is_enabled());
    g_unsetenv("GNC_COGNITIVE_ENABLED");
}

int
main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
