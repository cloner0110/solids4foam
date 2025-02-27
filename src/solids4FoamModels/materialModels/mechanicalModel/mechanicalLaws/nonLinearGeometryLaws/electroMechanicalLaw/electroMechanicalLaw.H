/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | foam-extend: Open Source CFD
   \\    /   O peration     |
    \\  /    A nd           | For copyright notice see file Copyright
     \\/     M anipulation  |
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

Class
    electroMechanicalLaw

Description
    Generalised electro-coupled mechanical law, where the stress is composed of
    two components: an active component, stemming from electro-mechanical
    coupling; and a passive component coming from a hyperelastic law.

    The passive hyperelastic law is run-time selectable.

SourceFiles
    electroMechanicalLaw.C

Author
    Philip Cardiff, UCD. All rights reserved.

\*---------------------------------------------------------------------------*/

#ifndef electroMechanicalLaw_H
#define electroMechanicalLaw_H

#include "mechanicalLaw.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                         Class electroMechanicalLaw Declaration
\*---------------------------------------------------------------------------*/

class electroMechanicalLaw
:
    public mechanicalLaw
{
    // Private Data

        //- Run-time selectable passive mechanical law
        autoPtr<mechanicalLaw> passiveMechLawPtr_;

        //- Constant active tension
        const dimensionedScalar Ta_;

    // Private Member Functions

        // //- Make the initial pore-pressure surface field
        // void makeP0f() const;

        // //- Return a reference to the initial pore-pressure surface field
        // const surfaceScalarField& p0f() const;

        // //- Lookup and return a reference to the pressure field
        // const volScalarField& lookupPressureField() const;

        //- Disallow default bitwise copy construct
        electroMechanicalLaw(const electroMechanicalLaw&);

        //- Disallow default bitwise assignment
        void operator=(const electroMechanicalLaw&);

public:

    //- Runtime type information
    TypeName("electroMechanicalLaw");

    // Static data members


    // Constructors

        //- Construct from dictionary
        electroMechanicalLaw
        (
            const word& name,
            const fvMesh& mesh,
            const dictionary& dict,
            const nonLinearGeometry::nonLinearType& nonLinGeom
        );


    // Destructor

        virtual ~electroMechanicalLaw();


    // Member Functions

        //- Return the implicit stiffness
        //  This is the diffusivity for the Laplacian term
        virtual tmp<volScalarField> impK() const;

        //- Return the bulk modulus field
        virtual tmp<volScalarField> bulkModulus() const;

        //- Calculate the stress
        virtual void correct(volSymmTensorField& sigma);

        //- Calculate the stress
        virtual void correct(surfaceSymmTensorField& sigma);
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
