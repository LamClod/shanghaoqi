#include <QTest>
#include "semantic/policy.h"
#include "semantic/request.h"
#include "semantic/capability.h"
#include "semantic/failure.h"

class TestPolicy : public QObject {
    Q_OBJECT

private slots:
    void testPreflightValid() {
        Policy policy;

        SemanticRequest req;
        req.kind = TaskKind::Conversation;
        req.target.logicalModel = QStringLiteral("gpt-4");

        CapabilityProfile profile;
        profile.adapterId = QStringLiteral("openai");
        profile.taskSupport[TaskKind::Conversation] = true;

        auto result = policy.preflight(req, profile);
        QVERIFY(result.has_value());
    }

    void testPreflightUnsupportedTask() {
        Policy policy;

        SemanticRequest req;
        req.kind = TaskKind::ImageGeneration;
        req.target.logicalModel = QStringLiteral("gpt-4");

        CapabilityProfile profile;
        profile.adapterId = QStringLiteral("openai");
        profile.taskSupport[TaskKind::Conversation] = true;
        // ImageGeneration not in taskSupport

        auto result = policy.preflight(req, profile);
        QVERIFY(!result.has_value());
        QCOMPARE(result.error().kind, ErrorKind::NotSupported);
    }

    void testPlanBasic() {
        Policy policy;

        SemanticRequest req;
        req.kind = TaskKind::Conversation;
        req.target.logicalModel = QStringLiteral("gpt-4");
        req.target.fallback.maxAttempts = 5;

        CapabilityProfile profile;
        profile.adapterId = QStringLiteral("openai");

        auto plan = policy.plan(req, profile);
        QCOMPARE(plan.targetModel, QStringLiteral("gpt-4"));
        QCOMPARE(plan.maxAttempts, 5);
    }

    void testRetryDecisionRetryable() {
        Policy policy;

        ExecutionPlan plan;
        plan.maxAttempts = 3;
        plan.retryableKinds = { ErrorKind::Unavailable, ErrorKind::Timeout };

        auto failure = DomainFailure::unavailable(QStringLiteral("service down"));
        auto decision = policy.nextRetry(plan, 1, failure);
        QVERIFY(decision.retry);
    }

    void testRetryDecisionMaxAttemptsReached() {
        Policy policy;

        ExecutionPlan plan;
        plan.maxAttempts = 3;

        auto failure = DomainFailure::unavailable(QStringLiteral("service down"));
        auto decision = policy.nextRetry(plan, 3, failure);
        QVERIFY(!decision.retry);
    }

    void testRetryDecisionNonRetryable() {
        Policy policy;

        ExecutionPlan plan;
        plan.maxAttempts = 3;
        plan.retryableKinds = { ErrorKind::Unavailable };

        auto failure = DomainFailure::unauthorized(QStringLiteral("bad key"));
        auto decision = policy.nextRetry(plan, 1, failure);
        QVERIFY(!decision.retry);
    }
};

QTEST_MAIN(TestPolicy)
#include "tst_policy.moc"
