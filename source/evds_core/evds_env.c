////////////////////////////////////////////////////////////////////////////////
/// @file
////////////////////////////////////////////////////////////////////////////////
/// Copyright (C) 2012-2013, Black Phoenix
/// All rights reserved.
///
/// Redistribution and use in source and binary forms, with or without
/// modification, are permitted provided that the following conditions are met:
///   - Redistributions of source code must retain the above copyright
///     notice, this list of conditions and the following disclaimer.
///   - Redistributions in binary form must reproduce the above copyright
///     notice, this list of conditions and the following disclaimer in the
///     documentation and/or other materials provided with the distribution.
///   - Neither the name of the author nor the names of the contributors may
///     be used to endorse or promote products derived from this software without
///     specific prior written permission.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
/// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
/// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
/// DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS BE LIABLE FOR ANY
/// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
/// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
/// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
/// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
/// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
/// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////
#include "evds.h"
#include "math.h"


////////////////////////////////////////////////////////////////////////////////
/// @brief Returns gravitational field in the given position.
///
/// This function iterates through all objects which can produce gravitational field.
/// Gravitational field is defined as:
/// \f[
///		\mathbf{g} = \frac{\mathbf{F}}{m} = -\nabla\Phi
/// \f]
///
/// This function searches for objects with the "planet" type. If planet has a custom callback
/// defined, the callback is used (some planets may provide a more detailed gravitational field
/// model).
///
/// The following variables may be used when computing gravitational field
/// (only the smallest sufficient set of these variables is required):
/// Variable			| Description
/// --------------------|--------------------------
/// mu					| Gravitational parameter of the planet (in \f$m^3 s^{-2}\f$)
/// mass				| Mass of the planet (in \f$kg\f$)
/// j2					| Second spherical harmonic \f$J_2\f$
/// radius				| Planet radius (required if 'j2' is specified)
/// rs					| Sphere of influence
/// gravitational_field | Function pointer to EVDS_Callback_GetGravitationalField
///
/// Gravitational field is returned as an acceleration vector in same coordinates as position.
/// If sphere of influence is defined, gravitational calculations for this planet will be omitted
/// when an object is outside of this sphere. This may be unwanted if small perturbations must
/// be accounted for.
///
/// Additionally the gravitational potential field in the current location is returned, unless
/// no pointer to write the value back is given.
///
/// The following equations are used for planets with spherical gravity:
/// \f{eqnarray*}{
///		\Phi &=& \frac{\mu}{r} \\
///		\mathbf{g} &=& -\frac{\mu}{r^2}
/// \f}
///
/// If perturbation factor \f$J_2\f$ is specified, the spherical harmonics are used to calculate
/// total gravitational field:
/// \f{eqnarray*}{
///		\Phi &=& -\frac{\mu}{r}[1 + 
///			\sum\limits_{n=2}^\infty (\frac{R}{\mu})^n
///			\sum\limits_{m=0}^n P_{nm} sin(\theta)
///				[C_{nm} cos(m \lambda) + S_{nm} sin(m \lambda)]
///			] \\
///		\mathbf{g} &=& -\nabla\Phi
/// \f}
/// where:
///  - \f$R\f$ is the planets radius.
///  - \f$P_{nm}\f$ is the Legendre associated function.
///  - \f$C_{nm}\f$, \f$S_{nm}\f$ are the spherical harmonics.
///  - \f$\theta\f$ is the geocentric latitude.
///  - \f$\lambda\f$ is the geocentric longitude.
///
/// If only \f$J_2\f$ harmonic remains, the spherical harmonics can be simplified using:
/// \f{eqnarray*}{
///		sin(\theta) &=& sin(arcsin(\frac{z}{\sqrt{x^2 + y^2 + z^2}}))
///			= \frac{z}{r} \\
///		n &=& 2 \\
///		C_{20} &=& J_2 \\
///		C_{21} &=& 0 \\
///		C_{22} &=& 0 \\
///		P_{20} &=& \frac{3}{2}
/// \f}
///
/// Therefore the equations for gravitational field become:
/// \f{eqnarray*}{
///		\Phi &=& -\frac{\mu}{r}\left[1 + (\frac{R}{\mu})^2 P_{20} \cdot sin(\theta)\cdot C_{20}\right] \\
///			&=& -\frac{\mu}{r}\left[1 + (\frac{R}{\mu})^2 \frac{3}{2} J_2 
///				\frac{z}{r} \right] \\
///		\mathbf{g}_x &=& -\frac{\partial \Phi}{\partial x} = \frac{R^2}{\mu} \frac{3}{2} J_2
///			\frac{x z}{r^4} -
///			\frac{x}{r^2} \Phi \\
///		\mathbf{g}_y &=& -\frac{\partial \Phi}{\partial y} = \frac{R^2}{\mu} \frac{3}{2} J_2
///			\frac{y z}{r^4} -
///			\frac{y}{r^2} \Phi \\
///		\mathbf{g}_z &=& -\frac{\partial \Phi}{\partial z} = \frac{R^2}{\mu} \frac{3}{2} J_2
///			\left[ \frac{1}{r} - \frac{z^2}{r^4} \right] -
///			\frac{z}{r^2} \Phi
/// \f}
/// 
///
/// These are the suggested values for solar system major bodies:
/// Name	| Mass (kg)						| \f$J_2\f$
/// --------|-------------------------------|----------------
/// Sun		| \f$1.99 \cdot 10^{30}\f$		| N/A
/// Mercury | \f$3.30 \cdot 10^{23}\f$		| \f$60 \cdot 10^{-6}\f$
/// Venus	| \f$4.87 \cdot 10^{24}\f$		| \f$4.458 \cdot 10^{-6}\f$
/// Earth	| \f$5.97 \cdot 10^{24}\f$		| \f$1.08263 \cdot 10^{-3}\f$
/// Mars	| \f$6.42 \cdot 10^{23}\f$		| \f$1.96045 \cdot 10^{-3}\f$
/// Jupiter	| \f$1.90 \cdot 10^{27}\f$		| \f$14.736 \cdot 10^{-3}\f$
/// Saturn	| \f$5.68 \cdot 10^{26}\f$		| \f$16.298 \cdot 10^{-3}\f$
/// Uranus	| \f$8.68 \cdot 10^{25}\f$		| \f$3.34343 \cdot 10^{-3}\f$
/// Neptune	| \f$1.02 \cdot 10^{26}\f$		| \f$3.411 \cdot 10^{-3}\f$
/// Moon	| \f$7.35 \cdot 10^{22}\f$		| \f$202.7 \cdot 10^{-6}\f$
///
/// @param[in] system Pointer to the system object
/// @param[in] position Position, in which gravity field must be calculated
/// @param[out] field Total gravitational field in the position
///
/// @returns Error code
/// @retval EVDS_OK Completed successfully
////////////////////////////////////////////////////////////////////////////////
int EVDS_Environment_GetGravitationalField(EVDS_SYSTEM* system, EVDS_VECTOR* position, EVDS_REAL* phi, EVDS_VECTOR* field) {
	EVDS_OBJECT* target_coordinates;
	SIMC_LIST* planets;
	SIMC_LIST_ENTRY* entry;
	EVDS_VECTOR total_field;
	EVDS_REAL total_phi;

	//Check input and fetch list of planets
	if (!system) return EVDS_ERROR_BAD_PARAMETER;
	if (!position) return EVDS_ERROR_BAD_PARAMETER;
	target_coordinates = position->coordinate_system;
	EVDS_System_GetObjectsByType(system,"planet",&planets);

	//Start accumulating total field and potential
	EVDS_Vector_Set(&total_field,EVDS_VECTOR_ACCELERATION,target_coordinates,0.0,0.0,0.0);
	total_phi = 0.0;

	//Iterate through all planets
	entry = SIMC_List_GetFirst(planets);
	while (entry) {
		EVDS_VARIABLE* mass_var; //Planet mass
		EVDS_REAL mass;
		EVDS_VARIABLE* mu_var; //Gravitational parameter
		EVDS_REAL mu;
		EVDS_VARIABLE* j2_var; //First spherical harmonic
		EVDS_REAL j2;
		EVDS_VARIABLE* radius_var; //Planet radius
		EVDS_REAL radius;
		EVDS_VARIABLE* rs_var; //Sphere of influence
		EVDS_REAL rs;
		EVDS_VARIABLE* callback_var; //Custom field
		EVDS_Callback_GetGravitationalField* callback;

		//Planet state vector
		EVDS_STATE_VECTOR planet_state;
		EVDS_OBJECT* planet = SIMC_List_GetData(planets,entry);

		//Initialize temporary vectors
		EVDS_REAL r2,r;
		EVDS_VECTOR G0,Gr,Gn,Ga;
		EVDS_REAL Gphi = 0.0;
		EVDS_Vector_Initialize(G0);
		EVDS_Vector_Initialize(Gr);
		EVDS_Vector_Initialize(Gn);
		EVDS_Vector_Initialize(Ga);

		//Get planet state and position in position vector coordinates
		EVDS_Object_GetStateVector(planet,&planet_state);
		EVDS_Vector_Convert(&G0,&planet_state.position,target_coordinates);

		//Get planets parameters
		if (EVDS_Object_GetVariable(planet,"mu",&mu_var) == EVDS_OK)		EVDS_Variable_GetReal(mu_var,&mu);
		else																mu_var = 0;
		if (EVDS_Object_GetVariable(planet,"j2",&j2_var) == EVDS_OK)		EVDS_Variable_GetReal(j2_var,&j2);
		else																j2_var = 0;
		if (EVDS_Object_GetVariable(planet,"mass",&mass_var) == EVDS_OK)	EVDS_Variable_GetReal(mass_var,&mass);
		else																mass_var = 0;
		if (EVDS_Object_GetVariable(planet,"radius",&radius_var) == EVDS_OK)EVDS_Variable_GetReal(radius_var,&radius);
		else																radius_var = 0;
		if (EVDS_Object_GetVariable(planet,"rs",&mass_var) == EVDS_OK)		EVDS_Variable_GetReal(rs_var,&rs);
		else																rs_var = 0;
		if (EVDS_Object_GetVariable(planet,"gravitational_field",&callback_var) == EVDS_OK) {
			EVDS_Variable_GetFunctionPointer(callback_var,(void**)(&callback));
		} else callback = 0;

		//Calculate radius-vector
		EVDS_Vector_Subtract(&Gr,position,&G0);
		EVDS_Vector_Dot(&r2,&Gr,&Gr);
		if (r2 == 0.0) r2 = EVDS_EPS;
		r = sqrtf(r2);

		//Check if outside of sphere of influence
		if (rs_var && (r2 > rs*rs)) {
			entry = SIMC_List_GetNext(planets,entry);
			continue; //Too far for gravity to have a meaningful influence
		}

		//Compute gravity acceleration from custom callback or stock code
		if (callback) {
			callback(planet,&Gr,&Gphi,&Ga);
			EVDS_Vector_Add(&total_field,&total_field,&Ga);
			total_phi += Gphi;
		} else {
			//Calculate mu for the planet
			if (!mu_var) {
				if (mass_var) {
					mu = 6.6738480e-11 * mass;
				} else {
					entry = SIMC_List_GetNext(planets,entry);
					continue; //Not enough information to compute gravity for this planet
				}
			}
			
			if (j2_var && radius_var) { //Non-spherical model
				EVDS_REAL x = Gr.x;
				EVDS_REAL y = Gr.y;
				EVDS_REAL z = Gr.z;
				EVDS_REAL sinlat = z/r;

				//Potential
				Gphi = -(mu/r)*(1 + (radius*radius/(mu*mu))*(3.0/2.0)*j2*sinlat);

				//Acceleration
				Ga.coordinate_system = Gr.coordinate_system;
				Ga.x = (radius*radius/mu)*(3.0/2.0)*j2*
					(x*z/(r2*r2)) -
					Gphi*x/r2;

				Ga.y = (radius*radius/mu)*(3.0/2.0)*j2*
					(y*z/(r2*r2)) -
					Gphi*y/r2;

				Ga.z = (radius*radius/mu)*(3.0/2.0)*j2*
					(1/r - z*z/(r2*r2)) -
					Gphi*z/r2;
			} { //Spherical model  else
				//Potential
				Gphi = -mu/r;

				//Acceleration
				EVDS_Vector_Normalize(&Gn,&Gr);
				EVDS_Vector_Multiply(&Ga,&Gn,-mu/r2);
			}

			//Reinterpret vector as acceleration, add to total acceleration
			Ga.derivative_level = EVDS_VECTOR_ACCELERATION;
			EVDS_Vector_Add(&total_field,&total_field,&Ga);
			total_phi += Gphi;
		}
		entry = SIMC_List_GetNext(planets,entry);
	}

	//Write back information
	if (phi) *phi = total_phi;
	if (field) EVDS_Vector_Copy(field,&total_field);
	return EVDS_OK;
}