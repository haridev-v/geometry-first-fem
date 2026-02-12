---
layout: default
title: Chapter 3 — Weak Forms as Operator Projections
parent: Table of Contents
nav_order: 2
---


# {{ page.title }}

---

## Contents
{: .no_toc .text-delta}
1. TOC
{:toc}

## 3.1 Weak Forms as Operator Projections

We start from the abstract operator equation

$$Lu = f \quad \text{in } \Omega.$$

Instead of solving this equation pointwise, we test it against functions  $v \in V$ and require

$$\langle Lu, v \rangle = \langle f, v \rangle.$$

This expresses the PDE as an operator relation inside an inner-product space.

We define

$$a(u,v) := \langle Lu, v \rangle, \qquad l(v) := \langle f, v \rangle.$$

Thus, the weak form becomes

$$a(u,v) = l(v) \quad \forall v \in V.$$

Here:- $a(\cdot,\cdot)$ is a bilinear form- $l(\cdot)$ is a linear functional

---

## 3.2 Bilinearity and Linear Functionals

The bilinear form $a(u,v)$ satisfies:
1. Linearity in the first argument$$a(u_1 + u_2, v) = a(u_1,v) + a(u_2,v)$$
2. Linearity in the second argument$$a(u, v_1 + v_2) = a(u,v_1) + a(u,v_2)$$

Similarly, the load functional satisfies

$$l(v_1 + v_2) = l(v_1) + l(v_2).$$

---

## 3.3 Weak Form Geometry
The weak form can be interpreted geometrically:
> Find $u \in V$ such that  > $$a(u,v) = l(v) \quad \forall v \in V.$$
This means that the residual $Lu - f$ is orthogonal to all test functions.
Thus, the solution is a projection in function space determined by the inner product geometry.

---

## 3.4 Finite-Dimensional Subspace Projection

We choose a finite-dimensional subspace

$$V_h = \text{span}\{\phi_1, \phi_2, \dots, \phi_N\}.$$

We approximate the solution as

$$u_h = \sum_{j=1}^N c_j \phi_j.$$

Enforcing the weak form on each basis function gives

$$a(u_h, \phi_i) = l(\phi_i), \quad i = 1,\dots,N.$$

Substituting $u_h$,

$$a\left(\sum_{j=1}^N c_j \phi_j, \phi_i\right) = l(\phi_i)$$

which yields

$$\sum_{j=1}^N c_j\, a(\phi_j, \phi_i) = l(\phi_i).$$

This is a linear system of $N$ equations in $N$ unknowns $c_j$.

---

## 3.5 Operator Matrix and Load Vector

We define:
1. Operator matrix$$K_{ij} := a(\phi_j, \phi_i)$$
2. Load vector$$b_i := l(\phi_i)$$

Then the system becomes

$$\sum_{j=1}^N K_{ij} c_j = b_i$$

or in matrix form

$$Kc = b.$$

The matrix $K$ is the stiffness matrix.

---

## 3.6 Weak Continuity and Higher-Order Spaces

$H^1$ finite element spaces are:- continuous in value ($C^0$)- discontinuous in gradient across elements
But $H^2$ problems require continuity of first derivatives.
This is geometrically admissible but algebraically demanding.
Three strategies exist:
1. $C^1$-conforming elements     Examples: Hermite elements, Argyris triangle, Bogner–Fox–Schmit rectangle     These enforce:   - function continuity   - gradient continuity
2. Mixed formulations     Example:   $$   \Delta^2 u = f   \Rightarrow   \begin{cases}   w = \Delta u \\   \Delta w = f   \end{cases}   $$   Each equation is second order, so standard $H^1$ FEM applies.
3. Weakly imposed continuity     Examples: discontinuous Galerkin, interior penalty methods     Idea:   - allow discontinuous gradients   - penalize jumps weakly in the variational form

---

## 3.7 Final Geometric Summary
- FEM is a projection method- Projection requires a well-defined inner product- Inner products define geometry- Geometry dictates function space compatibility
| Operator order | Weak form geometry | FEM status ||----------------|------------------|------------|| 1 | Value / flux | easy || 2 | gradient | standard || 3 | fractional curvature | incompatible || 4 | curvature | hard but possible |
---

## 3.8 The Bilinear Form

A form $a(u,v)$ is bilinear if:
1. It is linear in the first argument$$a(u_1+u_2, v) = a(u_1,v) + a(u_2,v)$$
2. It is linear in the second argument$$a(u, v_1+v_2) = a(u,v_1) + a(u,v_2)$$

Concept correspondence:
| Concept | Finite vectors | Functions / FEM ||---------|---------------|-----------------|| vector | $\tilde{x}\in \mathbb{R}^n$ | $u(x)$ || dot product | $x^T y$ | $\int uv$ || generalized dot | $x^T A y$ | $a(u,v)$ || matrix | $A$ | operator encoded by $a$ |

All weak forms can be written as
$$a(u_h, v) = l(v),$$
where $a(\cdot,\cdot)$ is bilinear and $l(\cdot)$ is linear.

---

## 3.9 From Weak Forms to Linear Systems

We start from

$$a(u_h, v) = l(v).$$

Let
$$V_h = \text{span}\{\phi_1,\phi_2,\dots,\phi_N\},$$

so
$$u_h = \sum_{j=1}^N c_j \phi_j.$$

Enforcing on basis functions:
$$a(u_h, \phi_i) = l(\phi_i)$$

$$\Rightarrow \sum_{j=1}^N c_j a(\phi_j,\phi_i) = l(\phi_i).$$

This produces the linear system

$$Kc = b.$$

---

## 3.10 Stiffness Matrix as Operator-Induced Geometry

The mass matrix encoded the geometry of the subspace.  The stiffness matrix encodes:
- geometry induced by the operator- energy norms- stability properties
Thus, matrices represent geometry in coordinates.
The mass matrix comes from expressing geometry in coordinates.  The stiffness matrix comes from expressing operator-induced geometry of the solution space in coordinates.

---

## 3.11 Boundary Conditions as Part of Geometry

In earlier chapters, projection lived entirely inside one space:- we had space $V$- we had an inner product- we projected onto a subspace $V_h$
This worked because the inner product was closed: no "leaks".
In PDEs, when derivatives are moved (integration by parts), geometry leaks into the boundary.

### 1. Dirichlet boundary conditions — geometry by restriction

We enforce
$$u = g \quad \text{on } \partial\Omega,$$
and test functions satisfy
$$v = 0 \quad \text{on } \partial\Omega.$$
Then boundary terms vanish, and the weak form becomes purely interior.
Dirichlet boundary conditions constrain the trial space.

### 2. Neumann boundary conditions — geometry by completion

We prescribe flux

$$\frac{\partial u}{\partial n} = q \quad \text{on } \partial\Omega.$$

Then the weak form becomes

$$\int_\Omega \nabla u \cdot \nabla v \, dx= \int_\Omega f v \, dx + \int_{\partial\Omega} q v \, ds.$$

Neumann conditions contribute to the load functional.

>Boundary conditions are not always “external loads”.  Some become loads after integration by parts, others reshape the geometry of the space.

---

## 3.12 Function Space Geometry
Up to now, functions were treated as vectors.  But vectors only become meaningful once a geometry is chosen.
Central question:> What does it mean for two functions to be close?

### 3.12.1 Induced Norms

Let $V$ be a vector space with inner product $\langle\cdot,\cdot\rangle$.  The induced norm is

$$\|u\|_V = \sqrt{\langle u,u\rangle}.$$

Distance follows as

$$d(u,v) = \|u-v\|_V = \sqrt{\langle u-v, u-v\rangle}.$$

Hierarchy:- inner product → norm → distance

Geometric quantities:- length: $\|u\| = \sqrt{\langle u,u\rangle}$- angle: $\cos\theta = \frac{\langle u,v\rangle}{\|u\|\|v\|}$- orthogonality: $\langle u,v\rangle = 0$

---

### 3.12.2 Value-Based Geometry (Amplitude Geometry)

On $V = L^2(\Omega)$ define

$$\langle u,v\rangle_{L^2} = \int_\Omega u(x)v(x)\,dx.$$

Induced norm:

$$\|u\|_{L^2} = \sqrt{\int_\Omega u(x)^2 dx}.$$

This geometry measures:- similarity in amplitude- alignment of function values- total energy

Functions that differ only by a constant may appear similar in certain contexts.

---

### 3.12.3 Variation-Based Geometry (Gradient Geometry)

Define

$$\langle u,v\rangle = \int_\Omega \nabla u \cdot \nabla v \, dx.$$

Induced norm:

$$\|u\| = \sqrt{\int_\Omega |\nabla u|^2 dx}.$$

This geometry measures:- similarity in variation- alignment of gradients- deformation or strain energy

---

### 3.12.4 Curvature-Based Geometry (Second-Order Geometry)

Define

$$\langle u,v\rangle = \int_\Omega \nabla^2 u : \nabla^2 v \, dx.$$

Induced norm:

$$\|u\| = \sqrt{\int_\Omega (\nabla^2 u)^2 dx}.$$

This geometry measures:- similarity in curvature- bending energy- higher-order smoothness

---

### 3.12.5 Directional (Transport-Weighted) Geometry

Let $\beta(x)$ be a given vector field. Define

$$\langle u,v\rangle = \int_\Omega (\beta \cdot \nabla u)(\beta \cdot \nabla v)\, dx.$$

Induced norm:

$$\|u\| = \sqrt{\int_\Omega (\beta \cdot \nabla u)^2 dx}.$$

This geometry measures:- variation along flow direction- transport-aligned similarity- convection-dominated behaviour

---

### 3.12.6 Generalization: Operator-Induced Geometry
Let $B$ be a linear operator.  We measure functions by applying the operator and then taking the $L^2$ norm:
$$\langle u,v\rangle_B = \int_\Omega (Bu)(Bv)\, dx.$$
Thus,
$$\|u\|_B = \|Bu\|_{L^2} = \sqrt{\int_\Omega (Bu)^2 dx}.$$
Hence, geometry is induced by the operator applied to the function.
The induced norm measures the length of a vector under a chosen geometry.  The induced distance measures how far apart two vectors are by measuring the length of their difference in that geometry.
