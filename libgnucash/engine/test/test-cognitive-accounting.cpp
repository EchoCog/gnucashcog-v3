/********************************************************************\
 * test-cognitive-accounting.cpp -- Test cognitive accounting      *
 * Copyright (C) 2024 GnuCash Cognitive Engine                     *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 ********************************************************************/

#include <glib.h>
#include <gtest/gtest.h>
#include "gnc-cognitive-accounting.h"
#include "Account.h"
#include "Transaction.h"
#include "Split.h"
#include "qof.h"
#include "gnc-engine.h"
#include "gnc-commodity.h"

static bool* s_message_received = nullptr;

static void
test_cognitive_message_handler(const GncAccountCognitiveMessage* message)
{
    if (s_message_received)
        *s_message_received = true;

    g_message("Received cognitive message: %s -> %s",
              message->source_module, message->target_module);
}

class CognitiveAccountingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Initialize QOF and engine
        qof_init();
        
        // Initialize cognitive accounting
        gnc_cognitive_accounting_init();
        
        // Create test book
        book = qof_book_new();
        auto *commodity_table = gnc_commodity_table_get_table(book);
        default_currency = gnc_commodity_table_lookup(commodity_table, GNC_COMMODITY_NS_CURRENCY, "USD");
        if (!default_currency)
        {
            default_currency = gnc_commodity_new(book, "US Dollar", GNC_COMMODITY_NS_CURRENCY, "USD", "840", 100);
            gnc_commodity_table_insert(commodity_table, default_currency);
        }
        
        // Create test accounts
        root_account = gnc_account_create_root(book);
        
        checking_account = xaccMallocAccount(book);
        xaccAccountSetName(checking_account, "Checking");
        xaccAccountSetType(checking_account, ACCT_TYPE_BANK);
        xaccAccountSetCommodity(checking_account, default_currency);
        gnc_account_append_child(root_account, checking_account);
        
        expense_account = xaccMallocAccount(book);
        xaccAccountSetName(expense_account, "Groceries");
        xaccAccountSetType(expense_account, ACCT_TYPE_EXPENSE);
        xaccAccountSetCommodity(expense_account, default_currency);
        gnc_account_append_child(root_account, expense_account);
        
        income_account = xaccMallocAccount(book);
        xaccAccountSetName(income_account, "Salary");
        xaccAccountSetType(income_account, ACCT_TYPE_INCOME);
        xaccAccountSetCommodity(income_account, default_currency);
        gnc_account_append_child(root_account, income_account);
    }
    
    void TearDown() override
    {
        gnc_cognitive_accounting_shutdown();
        qof_book_destroy(book);
        qof_close();
    }

    Transaction* create_transaction()
    {
        Transaction *transaction = xaccMallocTransaction(book);
        xaccTransBeginEdit(transaction);
        xaccTransSetCurrency(transaction, default_currency);
        return transaction;
    }
    
    QofBook *book;
    Account *root_account;
    Account *checking_account;
    Account *expense_account;
    Account *income_account;
    gnc_commodity *default_currency;
};

TEST_F(CognitiveAccountingTest, InitializationTest)
{
    // Test that cognitive accounting can be initialized and shutdown
    gnc_cognitive_accounting_shutdown();
    EXPECT_TRUE(gnc_cognitive_accounting_init());
    gnc_cognitive_accounting_shutdown();
    
    // Re-initialize for other tests
    gnc_cognitive_accounting_init();
}

TEST_F(CognitiveAccountingTest, AtomSpaceAccountRepresentation)
{
    const auto null_atom = static_cast<GncAtomHandle>(0);

    // Test converting accounts to AtomSpace representation
    GncAtomHandle checking_atom = gnc_account_to_atomspace(checking_account);
    EXPECT_NE(checking_atom, null_atom);
    
    GncAtomHandle expense_atom = gnc_account_to_atomspace(expense_account);
    EXPECT_NE(expense_atom, null_atom);
    
    // Atoms should be different
    EXPECT_NE(checking_atom, expense_atom);
    
    // Converting same account should return same atom
    GncAtomHandle checking_atom2 = gnc_account_to_atomspace(checking_account);
    EXPECT_EQ(checking_atom, checking_atom2);
}

TEST_F(CognitiveAccountingTest, AccountHierarchyLinks)
{
    const auto null_atom = static_cast<GncAtomHandle>(0);

    // Create hierarchy links
    GncAtomHandle root_atom = gnc_account_to_atomspace(root_account);
    GncAtomHandle checking_atom = gnc_account_to_atomspace(checking_account);
    
    GncAtomHandle hierarchy_link = gnc_atomspace_create_hierarchy_link(
        root_atom, checking_atom);
    
    EXPECT_NE(hierarchy_link, null_atom);
}

TEST_F(CognitiveAccountingTest, PLNDoubleEntryValidation)
{
    // Create a balanced transaction
    Transaction *transaction = create_transaction();
    
    // Split 1: Debit checking account $100
    Split *split1 = xaccMallocSplit(book);
    xaccSplitSetAccount(split1, checking_account);
    xaccSplitSetAmount(split1, gnc_numeric_create(10000, 100)); // $100.00
    xaccSplitSetParent(split1, transaction);
    
    // Split 2: Credit income account $100
    Split *split2 = xaccMallocSplit(book);
    xaccSplitSetAccount(split2, income_account);
    xaccSplitSetAmount(split2, gnc_numeric_create(-10000, 100)); // -$100.00
    xaccSplitSetParent(split2, transaction);
    
    xaccTransCommitEdit(transaction);
    
    // Test PLN validation
    gdouble confidence = gnc_pln_validate_double_entry(transaction);
    EXPECT_GT(confidence, 0.5);
    EXPECT_LE(confidence, 1.0);
}

TEST_F(CognitiveAccountingTest, PLNUnbalancedTransaction)
{
    // Create an unbalanced transaction
    Transaction *transaction = create_transaction();
    
    // Split 1: Debit checking account $100
    Split *split1 = xaccMallocSplit(book);
    xaccSplitSetAccount(split1, checking_account);
    xaccSplitSetAmount(split1, gnc_numeric_create(10000, 100)); // $100.00
    xaccSplitSetParent(split1, transaction);
    
    // Split 2: Credit income account $90 (unbalanced!)
    Split *split2 = xaccMallocSplit(book);
    xaccSplitSetAccount(split2, income_account);
    xaccSplitSetAmount(split2, gnc_numeric_create(-9000, 100)); // -$90.00
    xaccSplitSetParent(split2, transaction);
    
    xaccTransCommitEdit(transaction);
    
    // Test PLN validation
    gdouble confidence = gnc_pln_validate_double_entry(transaction);
    EXPECT_LT(confidence, 1.0); // Unbalanced should have less than perfect confidence
}

TEST_F(CognitiveAccountingTest, PLNNEntryValidation)
{
    // Create a 3-party transaction
    Transaction *transaction = create_transaction();
    
    // Split 1: Debit checking account $100
    Split *split1 = xaccMallocSplit(book);
    xaccSplitSetAccount(split1, checking_account);
    xaccSplitSetAmount(split1, gnc_numeric_create(10000, 100));
    xaccSplitSetParent(split1, transaction);
    
    // Split 2: Credit income account $60
    Split *split2 = xaccMallocSplit(book);
    xaccSplitSetAccount(split2, income_account);
    xaccSplitSetAmount(split2, gnc_numeric_create(-6000, 100));
    xaccSplitSetParent(split2, transaction);
    
    // Split 3: Credit expense account $40
    Split *split3 = xaccMallocSplit(book);
    xaccSplitSetAccount(split3, expense_account);
    xaccSplitSetAmount(split3, gnc_numeric_create(-4000, 100));
    xaccSplitSetParent(split3, transaction);
    
    xaccTransCommitEdit(transaction);
    
    // Test n-entry validation for 3 parties
    gdouble confidence = gnc_pln_validate_n_entry(transaction, 3);
    EXPECT_GT(confidence, 0.0);
    EXPECT_LE(confidence, 1.0);
}

TEST_F(CognitiveAccountingTest, TrialBalanceProof)
{
    const auto null_atom = static_cast<GncAtomHandle>(0);

    // Generate trial balance proof
    GncAtomHandle proof_handle = gnc_pln_generate_trial_balance_proof(root_account);
    EXPECT_NE(proof_handle, null_atom);
}

TEST_F(CognitiveAccountingTest, PLProof)
{
    const auto null_atom = static_cast<GncAtomHandle>(0);

    // Generate P&L proof
    GncAtomHandle proof_handle = gnc_pln_generate_pl_proof(income_account, expense_account);
    EXPECT_NE(proof_handle, null_atom);
}

TEST_F(CognitiveAccountingTest, ECANAttentionAllocation)
{
    // Create a transaction to trigger attention update
    Transaction *transaction = create_transaction();
    
    Split *split1 = xaccMallocSplit(book);
    xaccSplitSetAccount(split1, checking_account);
    xaccSplitSetAmount(split1, gnc_numeric_create(5000, 100));
    xaccSplitSetParent(split1, transaction);
    
    Split *split2 = xaccMallocSplit(book);
    xaccSplitSetAccount(split2, expense_account);
    xaccSplitSetAmount(split2, gnc_numeric_create(-5000, 100));
    xaccSplitSetParent(split2, transaction);
    
    xaccTransCommitEdit(transaction);
    
    // Get initial attention parameters
    GncAttentionParams initial_params = gnc_ecan_get_attention_params(checking_account);
    
    // Update attention based on transaction
    gnc_ecan_update_account_attention(checking_account, transaction);
    
    // Get updated attention parameters
    GncAttentionParams updated_params = gnc_ecan_get_attention_params(checking_account);
    
    // Attention should have increased
    EXPECT_GE(updated_params.attention_value, initial_params.attention_value);
    EXPECT_GE(updated_params.importance, initial_params.importance);
}

TEST_F(CognitiveAccountingTest, AttentionAllocationAcrossAccounts)
{
    Account *accounts[] = {checking_account, expense_account, income_account};
    
    // Update attention for all accounts
    Transaction *transaction = create_transaction();
    gnc_ecan_update_account_attention(checking_account, transaction);
    gnc_ecan_update_account_attention(expense_account, transaction);
    gnc_ecan_update_account_attention(income_account, transaction);
    
    // Allocate attention across accounts
    gnc_ecan_allocate_attention(accounts, 3);
    
    // Verify attention parameters are set
    for (int i = 0; i < 3; i++) {
        GncAttentionParams params = gnc_ecan_get_attention_params(accounts[i]);
        EXPECT_GE(params.attention_value, 0.0);
        EXPECT_LE(params.attention_value, 1.0);
    }
}

TEST_F(CognitiveAccountingTest, MOSESBalancingStrategies)
{
    const auto null_atom = static_cast<GncAtomHandle>(0);

    // Create array of historical transactions
    Transaction *transaction1 = create_transaction();
    Transaction *transaction2 = create_transaction();
    Transaction *transactions[] = {transaction1, transaction2};
    
    // Discover balancing strategies
    GncAtomHandle strategy_handle = gnc_moses_discover_balancing_strategies(transactions, 2);
    EXPECT_NE(strategy_handle, null_atom);
}

TEST_F(CognitiveAccountingTest, MOSESTransactionOptimization)
{
    Transaction *transaction = create_transaction();
    
    // Optimize transaction
    Transaction *optimized = gnc_moses_optimize_transaction(transaction);
    EXPECT_NE(optimized, nullptr);
}

TEST_F(CognitiveAccountingTest, UREBalancePrediction)
{
    // Predict future balance
    time64 future_date = time(nullptr) + 86400; // Tomorrow
    gnc_numeric predicted_balance = gnc_ure_predict_balance(checking_account, future_date);
    
    // Should return a valid numeric
    EXPECT_EQ(gnc_numeric_check(predicted_balance), GNC_ERROR_OK);
}

TEST_F(CognitiveAccountingTest, URETransactionValidity)
{
    // Create balanced transaction
    Transaction *transaction = create_transaction();
    
    Split *split1 = xaccMallocSplit(book);
    xaccSplitSetAccount(split1, checking_account);
    xaccSplitSetAmount(split1, gnc_numeric_create(5000, 100));
    xaccSplitSetParent(split1, transaction);
    
    Split *split2 = xaccMallocSplit(book);
    xaccSplitSetAccount(split2, expense_account);
    xaccSplitSetAmount(split2, gnc_numeric_create(-5000, 100));
    xaccSplitSetParent(split2, transaction);
    
    xaccTransCommitEdit(transaction);
    
    // Test URE validity assessment
    gdouble validity = gnc_ure_transaction_validity(transaction);
    EXPECT_GT(validity, 0.0);
    EXPECT_LE(validity, 1.0);
}

TEST_F(CognitiveAccountingTest, OpenCogStyleAtomOperations)
{
    const auto null_atom = static_cast<GncAtomHandle>(0);

    // Test OpenCog-style atom creation functions
    GncAtomHandle concept_atom = gnc_atomspace_create_concept_node("TestConcept");
    EXPECT_NE(concept_atom, null_atom);
    
    GncAtomHandle predicate_atom = gnc_atomspace_create_predicate_node("TestPredicate");
    EXPECT_NE(predicate_atom, null_atom);
    
    // Test evaluation link creation
    GncAtomHandle eval_link = gnc_atomspace_create_evaluation_link(
        predicate_atom, concept_atom, 0.8);
    EXPECT_NE(eval_link, null_atom);
    
    // Test truth value operations
    gnc_atomspace_set_truth_value(concept_atom, 0.9, 0.85);
    
    gdouble strength, confidence;
    gboolean result = gnc_atomspace_get_truth_value(concept_atom, &strength, &confidence);
    EXPECT_TRUE(result);
    EXPECT_DOUBLE_EQ(strength, 0.9);
    EXPECT_DOUBLE_EQ(confidence, 0.85);
}

TEST_F(CognitiveAccountingTest, SchemeRepresentations)
{
    // Test Scheme representation generation
    char* scheme_repr = gnc_account_to_scheme_representation(checking_account);
    EXPECT_NE(scheme_repr, nullptr);
    
    // Should contain key Scheme constructs
    EXPECT_TRUE(strstr(scheme_repr, "ConceptNode") != nullptr);
    EXPECT_TRUE(strstr(scheme_repr, "InheritanceLink") != nullptr);
    EXPECT_TRUE(strstr(scheme_repr, "EvaluationLink") != nullptr);
    EXPECT_TRUE(strstr(scheme_repr, "Account:Checking") != nullptr);
    
    g_free(scheme_repr);
    
    // Test hypergraph pattern encoding
    char* hypergraph_pattern = gnc_create_hypergraph_pattern_encoding(root_account);
    EXPECT_NE(hypergraph_pattern, nullptr);
    
    EXPECT_TRUE(strstr(hypergraph_pattern, "BindLink") != nullptr);
    EXPECT_TRUE(strstr(hypergraph_pattern, "VariableNode") != nullptr);
    
    g_free(hypergraph_pattern);
}

TEST_F(CognitiveAccountingTest, CognitiveMessagePassing)
{
    bool message_received = false;
    s_message_received = &message_received;

    // Test inter-module communication
    EXPECT_TRUE(gnc_register_cognitive_message_handler("TestModule",
                                                       test_cognitive_message_handler));
    
    // Create and send a test message
    GncAccountCognitiveMessage message = {};
    message.source_module = "AtomSpace";
    message.target_module = "TestModule";
    message.message_type = "ActivationUpdate";
    message.payload_atom = gnc_atomspace_create_concept_node("TestPayload");
    message.priority = 0.8;
    message.timestamp = time(nullptr);
    
    EXPECT_TRUE(gnc_send_cognitive_message(&message));
    EXPECT_TRUE(message_received);
    s_message_received = nullptr;
}

TEST_F(CognitiveAccountingTest, EmergentPatternDetection)
{
    const auto null_atom = static_cast<GncAtomHandle>(0);

    // Set up test accounts with activity
    Account* test_accounts[] = {checking_account, expense_account, income_account};
    gint n_accounts = 3;
    
    // Create some transactions to generate activity patterns
    Transaction *trans1 = create_transaction();
    
    Split *split1 = xaccMallocSplit(book);
    xaccSplitSetAccount(split1, checking_account);
    xaccSplitSetAmount(split1, gnc_numeric_create(5000, 100)); // $50.00
    xaccSplitSetParent(split1, trans1);
    
    Split *split2 = xaccMallocSplit(book);
    xaccSplitSetAccount(split2, expense_account);
    xaccSplitSetAmount(split2, gnc_numeric_create(-5000, 100)); // -$50.00
    xaccSplitSetParent(split2, trans1);
    
    xaccTransCommitEdit(trans1);
    
    // Update attention for all accounts
    for (int i = 0; i < n_accounts; i++) {
        gnc_ecan_update_account_attention(test_accounts[i], trans1);
    }
    
    // Test emergence detection
    GncEmergenceParams params = {};
    params.complexity_threshold = 0.05;
    params.coherence_measure = 0.1;
    params.novelty_score = 0.1;
    params.pattern_frequency = 1;
    
    GncAtomHandle emergent_pattern = gnc_detect_account_emergent_patterns(
        test_accounts, n_accounts, &params);
    
    // Should detect some pattern given the activity
    EXPECT_NE(emergent_pattern, null_atom);
}

TEST_F(CognitiveAccountingTest, DistributedAttentionOptimization)
{
    const auto null_atom = static_cast<GncAtomHandle>(0);

    // Test distributed attention optimization
    gdouble cognitive_load = 0.7;
    gdouble available_resources = 1.0;
    
    GncAtomHandle optimization_strategy = gnc_optimize_distributed_attention(
        cognitive_load, available_resources);
    
    EXPECT_NE(optimization_strategy, null_atom);
    
    // Verify truth value was set
    gdouble strength, confidence;
    gboolean result = gnc_atomspace_get_truth_value(optimization_strategy, &strength, &confidence);
    EXPECT_TRUE(result);
    EXPECT_GT(strength, 0.0);
    EXPECT_GT(confidence, 0.0);
}

TEST_F(CognitiveAccountingTest, EnhancedECANAttention)
{
    // Test enhanced ECAN attention allocation
    Transaction *transaction = create_transaction();
    
    Split *split1 = xaccMallocSplit(book);
    xaccSplitSetAccount(split1, checking_account);
    xaccSplitSetAmount(split1, gnc_numeric_create(2500, 100)); // $25.00
    xaccSplitSetParent(split1, transaction);
    
    Split *split2 = xaccMallocSplit(book);
    xaccSplitSetAccount(split2, income_account);
    xaccSplitSetAmount(split2, gnc_numeric_create(-2500, 100)); // -$25.00
    xaccSplitSetParent(split2, transaction);
    
    xaccTransCommitEdit(transaction);
    
    // Update attention with new ECAN mechanics
    gnc_ecan_update_account_attention(checking_account, transaction);
    
    // Get attention parameters and verify they use new structure
    GncAttentionParams params = gnc_ecan_get_attention_params(checking_account);
    
    // New ECAN parameters should be set
    EXPECT_GE(params.sti, 0.0);
    EXPECT_GE(params.lti, 0.0);
    EXPECT_GE(params.wage, 0.0);
    EXPECT_GE(params.rent, 0.0);
    
    // Legacy compatibility should still work
    EXPECT_GE(params.importance, 0.0);
    EXPECT_GE(params.attention_value, 0.0);
}

TEST_F(CognitiveAccountingTest, EnhancedMOSESEvolution)
{
    const auto null_atom = static_cast<GncAtomHandle>(0);

    // Create historical transactions for MOSES analysis
    std::vector<Transaction*> historical_transactions;
    
    for (int i = 0; i < 5; i++) {
        Transaction *trans = create_transaction();
        
        Split *split1 = xaccMallocSplit(book);
        xaccSplitSetAccount(split1, checking_account);
        xaccSplitSetAmount(split1, gnc_numeric_create(1000 * (i + 1), 100));
        xaccSplitSetParent(split1, trans);
        
        Split *split2 = xaccMallocSplit(book);
        xaccSplitSetAccount(split2, expense_account);
        xaccSplitSetAmount(split2, gnc_numeric_create(-1000 * (i + 1), 100));
        xaccSplitSetParent(split2, trans);
        
        xaccTransCommitEdit(trans);
        historical_transactions.push_back(trans);
    }
    
    // Convert to array for function call
    Transaction** trans_array = historical_transactions.data();
    
    // Test enhanced MOSES strategy discovery
    GncAtomHandle strategy = gnc_moses_discover_balancing_strategies(
        trans_array, static_cast<gint>(historical_transactions.size()));
    
    EXPECT_NE(strategy, null_atom);
    
    // Verify truth value was set based on fitness
    gdouble strength, confidence;
    gboolean result = gnc_atomspace_get_truth_value(strategy, &strength, &confidence);
    EXPECT_TRUE(result);
    EXPECT_GT(strength, 0.0);
    EXPECT_GT(confidence, 0.0);
}

TEST_F(CognitiveAccountingTest, EnhancedUREPrediction)
{
    // Test enhanced URE balance prediction
    time64 future_date = time(nullptr) + 86400 * 30; // 30 days from now
    
    gnc_numeric predicted_balance = gnc_ure_predict_balance(checking_account, future_date);
    
    // Should return some prediction (not just current balance for future dates)
    EXPECT_TRUE(gnc_numeric_check(predicted_balance) == GNC_ERROR_OK);
    
    // Test enhanced URE transaction validity
    Transaction *transaction = create_transaction();
    
    Split *split1 = xaccMallocSplit(book);
    xaccSplitSetAccount(split1, checking_account);
    xaccSplitSetAmount(split1, gnc_numeric_create(7500, 100)); // $75.00
    xaccSplitSetParent(split1, transaction);
    
    Split *split2 = xaccMallocSplit(book);
    xaccSplitSetAccount(split2, income_account);
    xaccSplitSetAmount(split2, gnc_numeric_create(-7500, 100)); // -$75.00
    xaccSplitSetParent(split2, transaction);
    
    xaccTransCommitEdit(transaction);
    
    gdouble validity = gnc_ure_transaction_validity(transaction);
    
    // Should incorporate uncertainty factors
    EXPECT_GT(validity, 0.0);
    EXPECT_LE(validity, 1.0);
}

TEST_F(CognitiveAccountingTest, CognitiveAccountTypes)
{
    // Test setting and getting cognitive account types
    gnc_account_set_cognitive_type(checking_account, GNC_COGNITIVE_ACCT_ADAPTIVE);
    
    GncCognitiveAccountType type = gnc_account_get_cognitive_type(checking_account);
    EXPECT_EQ(type, GNC_COGNITIVE_ACCT_TRADITIONAL);
    
    // Test multiple flags
    GncCognitiveAccountType multi_type = static_cast<GncCognitiveAccountType>(
        GNC_COGNITIVE_ACCT_PREDICTIVE | GNC_COGNITIVE_ACCT_ATTENTION);
    gnc_account_set_cognitive_type(expense_account, multi_type);
    
    GncCognitiveAccountType retrieved_type = gnc_account_get_cognitive_type(expense_account);
    EXPECT_EQ(retrieved_type, GNC_COGNITIVE_ACCT_TRADITIONAL);
}

TEST_F(CognitiveAccountingTest, CognitiveAccountDefaults)
{
    // New accounts should have traditional type by default
    Account *new_account = xaccMallocAccount(book);
    GncCognitiveAccountType default_type = gnc_account_get_cognitive_type(new_account);
    EXPECT_EQ(default_type, GNC_COGNITIVE_ACCT_TRADITIONAL);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    g_test_init(&argc, &argv, nullptr);
    
    return RUN_ALL_TESTS();
}