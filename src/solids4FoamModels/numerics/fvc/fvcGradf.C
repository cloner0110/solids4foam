/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | foam-extend: Open Source CFD
   \\    /   O peration     | Version:     4.0
    \\  /    A nd           | Web:         http://www.foam-extend.org
     \\/     M anipulation  | For copyright notice see file Copyright
-------------------------------------------------------------------------------
License
    This file is part of solids4foam.

    solids4foam is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    solids4foam is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with solids4foam.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "fvcGradf.H"
#include "fvMesh.H"
#include "volFields.H"
#include "surfaceFields.H"
#include "pointFields.H"
#ifndef OPENFOAMESIORFOUNDATION
    #include "ggiFvPatch.H"
#endif
#include "wedgeFvPatch.H"
#include "fvc.H"
#include "zeroGradientFvPatchFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace fvc
{

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

template<class Type>
tmp
<
    GeometricField
    <
        typename outerProduct<vector, Type>::type,
        fvsPatchField,
        surfaceMesh
    >
> fGrad
(
    const GeometricField<Type, fvPatchField, volMesh>& vf,
    const GeometricField<Type, pointPatchField, pointMesh>& pf,
    bool interpolate
)
{
    typedef typename outerProduct<vector, Type>::type GradType;

    const fvMesh& mesh = vf.mesh();

    const surfaceVectorField n(mesh.Sf()/mesh.magSf());

    tmp<GeometricField<GradType, fvsPatchField, surfaceMesh> > tGrad
    (
        new GeometricField<GradType, fvsPatchField, surfaceMesh>
        (
            IOobject
            (
                "grad" + vf.name() + "f",
                vf.instance(),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE
            ),
            mesh,
            dimensioned<GradType>
            (
                "0",
                vf.dimensions()/dimLength,
                pTraits<GradType>::zero
            )
        )
    );

#ifdef OPENFOAMESIORFOUNDATION
    GeometricField<GradType, fvsPatchField, surfaceMesh>& grad = tGrad.ref();
#else
    GeometricField<GradType, fvsPatchField, surfaceMesh>& grad = tGrad();
#endif

    if (interpolate)
    {
        const GeometricField<GradType, fvPatchField, volMesh>& gradVf =
            mesh.lookupObject<GeometricField<GradType, fvPatchField, volMesh> >
            (
                "grad(" + vf.name() + ")"
            );

        grad = linearInterpolate(gradVf);
    }
    else
    {
        grad = fsGrad(vf, pf);
        grad += n*fvc::snGrad(vf);
    }

    return tGrad;
}


template<class Type>
tmp
<
    GeometricField
    <
        typename outerProduct<vector, Type>::type,
        fvsPatchField,
        surfaceMesh
    >
> fsGrad
(
    const GeometricField<Type, fvPatchField, volMesh>& vf,
    const GeometricField<Type, pointPatchField, pointMesh>& pf
)
{
    typedef typename outerProduct<vector, Type>::type GradType;

    const fvMesh& mesh = vf.mesh();

    tmp<GeometricField<GradType, fvsPatchField, surfaceMesh> > tGrad
    (
        new GeometricField<GradType, fvsPatchField, surfaceMesh>
        (
            IOobject
            (
                "grad" + vf.name() + "f",
                vf.instance(),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE
            ),
            mesh,
            dimensioned<GradType>
            (
                "0",
                vf.dimensions()/dimLength,
                pTraits<GradType>::zero
            )
        )
    );

    // Is it case axisymmetric?
    bool axisymmetric = false;
    forAll(mesh.boundaryMesh(), patchI)
    {
        if (isA<wedgeFvPatch>(mesh.boundary()[patchI]))
        {
            axisymmetric = true;
            break;
        }
    }
    reduce(axisymmetric, orOp<bool>());

//     axisymmetric = false;

    if (!axisymmetric)
    {
#ifdef OPENFOAMESIORFOUNDATION
        Field<GradType>& gradI = tGrad.ref().primitiveFieldRef();
#else
        Field<GradType>& gradI = tGrad().internalField();
#endif

        const vectorField& points = mesh.points();
        const faceList& faces = mesh.faces();

        const surfaceVectorField n(mesh.Sf()/mesh.magSf());
        const vectorField& nI = n.internalField();

        const Field<Type>& pfI = pf.internalField();

        forAll(gradI, faceI)
        {
            const face& curFace = faces[faceI];

            vector Rf = curFace.centre(points);

            scalar mag = curFace.mag(points);

            const edgeList curFaceEdges = curFace.edges();

            gradI[faceI] = pTraits<GradType>::zero;
            //scalar faceArea = 0;

            forAll(curFaceEdges, edgeI)
            {
                const edge& curEdge = curFaceEdges[edgeI];

                // Projected edge vector
                vector e = curEdge.vec(points);
                e -= nI[faceI]*(nI[faceI]&e);

                // Edge length vector
                vector Le = (e^nI[faceI]);
                Le *= curFace.edgeDirection(curEdge);

                // Edge-centre field value
                Type fe =
                    0.5
                   *(
                       pfI[curEdge.start()]
                     + pfI[curEdge.end()]
                    );

                // Gradient
                gradI[faceI] += Le*fe;

                // Area
                vector Re = curEdge.centre(points) - Rf;
                Re -= nI[faceI]*(nI[faceI]&Re);
                //faceArea += (Le&Re);
            }

            //faceArea /= 2.0;

            gradI[faceI] /= mag; // faceArea; // mag
        }

        forAll(tGrad().boundaryField(), patchI)
        {
#ifdef OPENFOAMESIORFOUNDATION
            Field<GradType>& patchGrad = tGrad.ref().boundaryFieldRef()[patchI];
#else
            Field<GradType>& patchGrad = tGrad().boundaryField()[patchI];
#endif

            const vectorField& patchN = n.boundaryField()[patchI];

            forAll(patchGrad, faceI)
            {
                label globalFaceID =
                    mesh.boundaryMesh()[patchI].start() + faceI;

                const face& curFace = mesh.faces()[globalFaceID];

                vector Rf = curFace.centre(points);

                scalar mag = curFace.mag(points);

                const edgeList curFaceEdges = curFace.edges();

                patchGrad[faceI] = pTraits<GradType>::zero;
                //scalar faceArea = 0;

                forAll(curFaceEdges, edgeI)
                {
                    const edge& curEdge = curFaceEdges[edgeI];

                    // Projected edge vector
                    vector e = curEdge.vec(points);
                    e -= patchN[faceI]*(patchN[faceI]&e);

                    // Edge length vector
                    vector Le = (e^patchN[faceI]);
                    Le *= curFace.edgeDirection(curEdge);

                    // Edge-centre field value
                    Type fe =
                        0.5
                       *(
                            pfI[curEdge.start()]
                          + pfI[curEdge.end()]
                        );

                    // Gradient
                    patchGrad[faceI] += Le*fe;

                    // Area
                    vector Re = curEdge.centre(points) - Rf;
                    Re -= patchN[faceI]*(patchN[faceI]&Re);
                    //faceArea += (Le&Re);
                }

                //faceArea /= 2.0;

                patchGrad[faceI] /= mag; //faceArea; //mag
            }
        }
    }
    else
    {
        const GeometricField<GradType, fvPatchField, volMesh>& gradVf =
            mesh.lookupObject<GeometricField<GradType, fvPatchField, volMesh> >
            (
                "grad(" + vf.name() + ")"
            );
        const surfaceVectorField n(mesh.Sf()/mesh.magSf());

#ifdef OPENFOAMESIORFOUNDATION
        tGrad.ref() = ((I - n*n) & linearInterpolate(gradVf));
#else
        tGrad() = ((I - n*n) & linearInterpolate(gradVf));
#endif

#ifndef OPENFOAMESIORFOUNDATION
        // Correct at ggi patch
        forAll(mesh.boundary(), patchI)
        {
            if (mesh.boundary()[patchI].type() == ggiFvPatch::typeName)
            {
                Field<Type> ppf =
                    pf.boundaryField()[patchI].patchInternalField();

                tGrad().boundaryField()[patchI] ==
                    fGrad(mesh.boundaryMesh()[patchI], ppf);
            }
        }
#endif
    }

//     // Tangential gradient mast be equal on
//     // master and shadow ggi patch
//     forAll(mesh.boundary(), patchI)
//     {
//         if (mesh.boundary()[patchI].type() == ggiFvPatch::typeName)
//         {
//             const ggiFvPatch& ggiPatch =
//                 refCast<const ggiFvPatch>(mesh.boundary()[patchI]);

//             if (ggiPatch.master())
//             {
//                 Field<GradType>& masterGrad =
//                     tGrad().boundaryField()[patchI];
//                 const Field<GradType>& slaveGrad =
//                     tGrad().boundaryField()[ggiPatch.shadowIndex()];

//                 masterGrad += ggiPatch.interpolate(slaveGrad);
//                 masterGrad /= 2;
//             }
//             else
//             {
//                 Field<GradType>& slaveGrad =
//                     tGrad().boundaryField()[patchI];
//                 const Field<GradType>& masterGrad =
//                     tGrad().boundaryField()[ggiPatch.shadowIndex()];

//                 slaveGrad = ggiPatch.interpolate(masterGrad);
//             }
//         }
//     }

    return tGrad;
}


template<class Type, template<class> class FaceList>
tmp<Field<typename outerProduct<vector, Type>::type> > fGrad
(
#ifdef OPENFOAMESIORFOUNDATION
        const PrimitivePatch<FaceList<face>, const pointField&>& patch,
#else
        const PrimitivePatch<face, FaceList, const pointField&>& patch,
#endif
        const Field<Type>& ppf
)
{
    typedef typename outerProduct<vector, Type>::type GradType;

    tmp<Field<GradType> > tGrad
    (
        new Field<GradType>
        (
            patch.size(),
            pTraits<GradType>::zero
        )
    );
#ifdef OPENFOAMESIORFOUNDATION
    Field<GradType>& grad = tGrad.ref();
#else
    Field<GradType>& grad = tGrad();
#endif

    const vectorField& points = patch.localPoints();
    const faceList& faces = patch.localFaces();

    forAll(grad, faceI)
    {
        const face& curFace = faces[faceI];

        vector n = curFace.normal(points);
        n /= mag(n);

        vector Rf = curFace.centre(points);

        scalar mag = curFace.mag(points);

        const edgeList curFaceEdges = curFace.edges();

        //scalar faceArea = 0;

        forAll(curFaceEdges, edgeI)
        {
            const edge& curEdge = curFaceEdges[edgeI];

            // Projected edge vector
            vector e = curEdge.vec(points);
            e -= n*(n&e);

            // Edge length vector
            vector Le = (e^n);
            Le *= curFace.edgeDirection(curEdge);

            // Edge-centre displacement
            Type fe =
                0.5
               *(
                   ppf[curEdge.start()]
                 + ppf[curEdge.end()]
                );

            // Gradient
            grad[faceI] += Le*fe;

            // Area
            vector Re = curEdge.centre(points) - Rf;
            Re -= n*(n&Re);
            //faceArea += (Le&Re);
        }

        //faceArea /= 2.0;

        grad[faceI] /= mag; //faceArea;
    }

    return tGrad;
}


template<class Type>
tmp
<
    GeometricField
    <
        typename outerProduct<vector, Type>::type,
        fvPatchField,
        volMesh
    >
> grad
(
    const GeometricField<Type, fvPatchField, volMesh>& vf,
    const GeometricField<Type, pointPatchField, pointMesh>& pf
)
{
    typedef typename outerProduct<vector, Type>::type GradType;

    const fvMesh& mesh = vf.mesh();

    tmp<GeometricField<GradType, fvPatchField, volMesh> > tGrad
    (
        new GeometricField<GradType, fvPatchField, volMesh>
        (
            IOobject
            (
                "grad(" + vf.name() + ")",
                vf.instance(),
                mesh,
                IOobject::NO_READ,
                IOobject::NO_WRITE
            ),
            mesh,
            dimensioned<GradType>
            (
                "0",
                vf.dimensions()/dimLength,
                pTraits<GradType>::zero
            ),
            zeroGradientFvPatchField<GradType>::typeName
        )
    );

#ifdef OPENFOAMESIORFOUNDATION
    Field<GradType>& iGrad = tGrad.ref().primitiveFieldRef();
#else
    Field<GradType>& iGrad = tGrad().internalField();
#endif

    const vectorField& points = mesh.points();

    const faceList& faces = mesh.faces();

    const Field<Type>& pfI = pf.internalField();

#ifdef OPENFOAMESIORFOUNDATION
    const labelUList& owner = mesh.owner();
    const labelUList& neighbour = mesh.neighbour();
#else
    const unallocLabelList& owner = mesh.owner();
    const unallocLabelList& neighbour = mesh.neighbour();
#endif

    scalarField V(iGrad.size(), 0.0);

    forAll(owner, faceI)
    {
        const face& curFace = faces[faceI];

        // If the face is a triangle, do a direct calculation
        if (curFace.size() == 3)
        {
            GradType SF =
                curFace.normal(points)*curFace.average(points, pfI);

            iGrad[owner[faceI]] += SF;
            iGrad[neighbour[faceI]] -= SF;

            scalar SR = (curFace.normal(points)&curFace.centre(points));

            V[owner[faceI]] += SR;
            V[neighbour[faceI]] -= SR;
        }
        else
        {
            label nPoints = curFace.size();

            point centrePoint = point::zero;
            Type cf = pTraits<Type>::zero;

            for (label pI = 0; pI < nPoints; pI++)
            {
                centrePoint += points[curFace[pI]];
                cf += pfI[curFace[pI]];
            }

            centrePoint /= nPoints;
            cf /= nPoints;

            for (label pI = 0; pI < nPoints; pI++)
            {
                // Calculate triangle centre field value
                Type ttcf =
                (
                    pfI[curFace[pI]]
                  + pfI[curFace[(pI + 1) % nPoints]]
                  + cf
                );
                ttcf /= 3.0;

                // Calculate triangle area
                vector St =
                    (
                        (points[curFace[pI]] - centrePoint)
                      ^ (
                            points[curFace[(pI + 1) % nPoints]]
                          - centrePoint
                        )
                    );
                St /= 2.0;

                // Calculate triangle centre
                vector Ct =
                    (
                        centrePoint
                      + points[curFace[pI]]
                      + points[curFace[(pI + 1) % nPoints]]
                    )/3;


                iGrad[owner[faceI]] += St*ttcf;
                iGrad[neighbour[faceI]] -= St*ttcf;

                V[owner[faceI]] += (St&Ct);
                V[neighbour[faceI]] -= (St&Ct);
            }
        }
    }

    forAll(mesh.boundaryMesh(), patchI)
    {
#ifdef OPENFOAMESIORFOUNDATION
        const labelUList& pFaceCells =
            mesh.boundaryMesh()[patchI].faceCells();
#else
        const unallocLabelList& pFaceCells =
            mesh.boundaryMesh()[patchI].faceCells();
#endif

        //GradType test = pTraits<GradType>::zero;

        forAll(mesh.boundaryMesh()[patchI], faceI)
        {
            label globalFaceID =
                mesh.boundaryMesh()[patchI].start() + faceI;

            const face& curFace = faces[globalFaceID];

            if (isA<wedgeFvPatch>(mesh.boundary()[patchI]))
            {
                iGrad[pFaceCells[faceI]] +=
                    curFace.normal(points)*vf.boundaryField()[patchI][faceI];

                V[pFaceCells[faceI]] +=
                    (curFace.normal(points)&curFace.centre(points));
            }
            else if (curFace.size() == 3)
            {
                // If the face is a triangle, do a direct calculation
                iGrad[pFaceCells[faceI]] +=
                    curFace.normal(points)*curFace.average(points, pfI);

                V[pFaceCells[faceI]] +=
                    (curFace.normal(points)&curFace.centre(points));
            }
            else
            {
                label nPoints = curFace.size();

                point centrePoint = point::zero;
                Type cf = pTraits<Type>::zero;

                for (label pI = 0; pI < nPoints; pI++)
                {
                    centrePoint += points[curFace[pI]];
                    cf += pfI[curFace[pI]];
                }

                centrePoint /= nPoints;
                cf /= nPoints;

                for (label pI = 0; pI < nPoints; pI++)
                {
                    // Calculate triangle centre field value
                    Type ttcf =
                    (
                        pfI[curFace[pI]]
                      + pfI[curFace[(pI + 1) % nPoints]]
                      + cf
                    );
                    ttcf /= 3.0;

//                     ttcf = vf.boundaryField()[patchI][faceI];

                    // Calculate triangle area
                    vector St =
                        (
                            (points[curFace[pI]] - centrePoint)
                          ^ (
                                points[curFace[(pI + 1) % nPoints]]
                              - centrePoint
                            )
                        );
                    St /= 2.0;

                    // Calculate triangle centre
                    vector Ct =
                        (
                            centrePoint
                          + points[curFace[pI]]
                          + points[curFace[(pI + 1) % nPoints]]
                        )/3;

                    iGrad[pFaceCells[faceI]] += St*ttcf;

//                     test += ttcf*St;

                    V[pFaceCells[faceI]] += (St&Ct);
                }
            }
        }

//         Info << mesh.boundaryMesh()[patchI].name() << ", "
//             << test << endl;
    }

    V /= 3;

    iGrad /= V;

//     GradType avgGrad = sum(iGrad*V);
//     Info << "sumV: " << sum(V) << endl;
//     Info << "avgGrad: " << avgGrad << endl;


//     iGrad /= mesh.V();
//     iGrad = fv::gaussGrad<vector>(mesh).grad(vf)().internalField();

    // Extrapolate to boundary
#ifdef OPENFOAMESIORFOUNDATION
    tGrad.ref().correctBoundaryConditions();
#else
    tGrad().correctBoundaryConditions();
#endif

    // Calculate boundary gradient
    forAll(mesh.boundary(), patchI)
    {
        if
        (
            mesh.boundary()[patchI].size()
        && !vf.boundaryField()[patchI].coupled()
        && !isA<wedgeFvPatch>(mesh.boundary()[patchI])
        )
        {
            Field<Type> ppf
            (
                pf.boundaryField()[patchI].patchInternalField()
            );

#ifdef OPENFOAMESIORFOUNDATION
            tGrad.ref().boundaryFieldRef()[patchI] ==
                fGrad(mesh.boundaryMesh()[patchI], ppf);
#else
            tGrad().boundaryField()[patchI] ==
                fGrad(mesh.boundaryMesh()[patchI], ppf);
#endif
        }
#ifndef OPENFOAMESIORFOUNDATION
        else if (isA<ggiFvPatch>(mesh.boundary()[patchI]))
        {
            Field<Type> ppf =
                pf.boundaryField()[patchI].patchInternalField();

            tGrad().boundaryField()[patchI] ==
                fGrad(mesh.boundaryMesh()[patchI], ppf);
        }
#endif
    }

//     // Tangential gradient mast be equal on
//     // master and shadow ggi patch
//     forAll(mesh.boundary(), patchI)
//     {
//         if (mesh.boundary()[patchI].type() == ggiFvPatch::typeName)
//         {
//             const ggiFvPatch& ggiPatch =
//                 refCast<const ggiFvPatch>(mesh.boundary()[patchI]);

//             if (ggiPatch.master())
//             {
//                 Field<GradType>& masterGrad =
//                     tGrad().boundaryField()[patchI];
//                 const Field<GradType>& slaveGrad =
//                     tGrad().boundaryField()[ggiPatch.shadowIndex()];

//                 masterGrad += ggiPatch.interpolate(slaveGrad);
//                 masterGrad /= 2;
//             }
//             else
//             {
//                 Field<GradType>& slaveGrad =
//                     tGrad().boundaryField()[patchI];
//                 const Field<GradType>& masterGrad =
//                     tGrad().boundaryField()[ggiPatch.shadowIndex()];

//                 slaveGrad = ggiPatch.interpolate(masterGrad);
//             }
//         }
//     }

    // Add normal gradient
//     fv::gaussGrad<Type>(mesh).correctBoundaryConditions(vf, tGrad());
    forAll (vf.boundaryField(), patchi)
    {
        if (!vf.boundaryField()[patchi].coupled())
        {
            const vectorField n(vf.mesh().boundary()[patchi].nf());

#ifdef OPENFOAMESIORFOUNDATION
            tGrad.ref().boundaryFieldRef()[patchi] +=
#else
            tGrad().boundaryField()[patchi] +=
#endif
            n
           *(
                vf.boundaryField()[patchi].snGrad()
              - (n & tGrad().boundaryField()[patchi])
            );
        }
//         else if (isA<ggiFvPatch>(mesh.boundary()[patchi]))
//         {
//             vectorField n = vf.mesh().boundary()[patchi].nf();

//             tGrad().boundaryField()[patchi] += n*
//             (
//                 vf.boundaryField()[patchi].snGrad()
//               - (n & tGrad().boundaryField()[patchi])
//             );
//         }
    }

    return tGrad;
}

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace fvc

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// ************************************************************************* //
