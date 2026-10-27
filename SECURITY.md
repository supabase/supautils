# Security Policy

Thank you for helping keep supautils and its users safe.

## Reporting a vulnerability

Please report suspected vulnerabilities through our HackerOne program:
<https://hackerone.com/supabase>. Please do not open public GitHub issues or
pull requests for security problems.

This repository contains supautils, a Postgres extension that lets roles that
are not superusers carry out selected privileged tasks, such as installing an
allowed set of extensions, which supautils then creates as a superuser. It is
how customers on the hosted Supabase platform manage their database without
holding superuser, and it can be installed in any Postgres. We accept reports
for both, and we triage them differently (see "Hosted vs. standalone" below).

## What we consider a vulnerability

In general, we want to hear about anything that lets a role that is not a
superuser use supautils to gain superuser, or to run code or SQL of its
choosing with the privileges supautils delegates to. The problem should be
present on the hosted platform or in a default configuration.

Examples of what this includes (these are illustrations, not a complete list):

- **Attacker-controlled code in a privileged install.** Getting supautils to
  run an extension script, or other SQL, that a lower role controls, with
  superuser rights. For example, a user-defined extension that takes the
  name of an allowed privileged extension.
- **Delegated privileges that go too far.** A privilege that supautils grants
  or allows, on its own or as configured on the hosted platform, that leads
  to superuser.

## What we do not consider a vulnerability

Reports that rely on the behavior below will normally be closed as
informative or not applicable.

**By design**

- **Non-superuser roles can install allowed extensions.** That is what
  supautils is for. It applies to every role in the database, not only to
  `postgres`, and it cannot be restricted to specific roles. A role
  installing an allowed extension is not by itself an escalation. Show what
  the role gains that it should not have.
- **Extension DDL run through supautils does not fire your event triggers.**
  This is a limitation of how supautils delegates the install, not a bypass
  of a security control.

**Prerequisites that make the attack pointless**

- The attacker already holds superuser, or needs to change the supautils
  configuration first.

## Out of scope for every Supabase asset

These are excluded program-wide (see also
<https://supabase.com/.well-known/security.txt>). They are repeated here so
you do not need to leave this repository to find them.

- Clickjacking on pages with no sensitive actions.
- Unauthenticated, logout and login CSRF.
- Attacks requiring MITM or physical access to a user's device.
- Attacks requiring social engineering.
- Any activity that could lead to the disruption of our service (DoS).
  This means volumetric attacks, load testing and flooding. A vulnerability in
  our code that lets an unprivileged attacker crash a service or exhaust
  resources for other users or tenants is in scope. Denial of service that only
  affects the attacker's own project, or that needs admin or owner privileges,
  is not.
- Content spoofing and text injection issues without showing an attack vector,
  or without being able to modify HTML/CSS.
- Email spoofing.
- Missing DNSSEC, CAA or CSP headers.
- Lack of Secure or HttpOnly flags on non-sensitive cookies.
- Dead links.
- User enumeration.

## Known and documented behavior

- **The set of privileged extensions** comes from the
  `supautils.privileged_extensions` setting described in the README. On the
  hosted platform, customers cannot change it.

## Hosted vs. standalone

On the hosted platform, customers do not get superuser. Superuser is reserved
for Supabase, and privileged extensions are created by Supabase's own
administrative role. Reaching superuser is valid, and what it leads to decides
how it is rated. On your own install, the
boundary depends on how you configure supautils. Say which one you
tested, and list your supautils settings if you did not use the hosted
platform.

## How to write a report that gets triaged quickly

- **Say what you tested, at the top of the report and in the title.** Use one of
  these labels, for example `[Production] ...` in the title and a "Target" line
  at the start of the description:
  - **Production**: the hosted Supabase platform, against a project you own.
    Include the project ref and the role you connected as. Never test another
    customer's project.
  - **Standalone, default configuration**: supautils from this repository,
    installed as the README describes. Include the supautils and Postgres
    versions and your supautils settings.
  - **Custom configuration**: you changed settings or code. List every change
    and why. We triage these lower.

  Reports that do not say which of the three they are about are slower to
  triage, and are often closed with a request to retest. A finding that works
  only on a custom configuration is not a finding against production.

- **Give a short, runnable PoC.** The exact SQL, run as the role you name, and
  output showing the role you ended up with. Once you have shown superuser,
  stop and report.
- **State the attacker's starting position and the boundary you crossed.**
  Which role do they start as, and which privilege do they reach?
- **Retest on the latest release.** Hosted projects receive new supautils
  releases as they are published.
- **Include versions.** The supautils version, the Postgres version, and the
  extensions involved.
- **One issue per report.** Link variants of the same root cause instead of
  filing each variant separately.
- **Do not send unreviewed AI output.** Reports that were not reproduced by a
  human and that ignore the documentation are closed. AI-assisted analysis is
  fine when you have verified the result.
- **Respond to follow-up questions.** Reports that go quiet after we ask for a
  reproduction are closed.

## Severity

We rate severity by realistic impact. Reaching superuser on the hosted
platform is valid, but is usually rated lower than its raw description
suggests, because the rating follows what it leads to. Escaping to the host
or to another project is rated higher.

## Testing and disclosure

These are our program-wide rules. They also apply to AI agents and automated
tools acting on your behalf.

**Testing**

- Do not run automated scanners against other customers' projects. Scanners can
  run up costs for our users, can disrupt services, and our own security
  systems cannot tell hostile reconnaissance from research. If you want to run
  an automated scanner, notify security@supabase.com first and run it only on
  your own Supabase project.
- Do not attack projects that belong to other customers.
- Do not take advantage of a vulnerability you find, for example by downloading
  more data than necessary to demonstrate it, or by deleting or modifying other
  people's data.

**Disclosure**

- Do not reveal the problem to others until we have researched and fixed it and
  informed affected customers.
- If you want to share your research publicly (a conference, a blog or any
  other public forum), send us a draft for review and approval at least 30 days
  before the publication date. Do not include data about Supabase customer
  projects, customers' data, or information about Supabase employees,
  contractors or partners.

**What we promise**

- We will respond to your report within 5 business days with our evaluation and
  an expected resolution date.
- If you have followed these instructions, we will not take legal action against
  you in regard to the report.
- We will handle your report with strict confidentiality and will not pass on
  your personal details to third parties without your permission.
- We will keep you informed of our progress.
- In public information about the problem, we will name you as the discoverer
  unless you prefer otherwise.
