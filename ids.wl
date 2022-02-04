(* ::Package:: *)

(* ::Input::Initialization:: *)
BeginPackage["IDS`"];


EstablishLink::usage = "EstablishLink[n]";
CloseLink::usage = "CloseLink[]";
SetCameraProperty::usage = "SetCameraProperty[name, value]";
SetImageDimensions::usage = "SetImageDimensions[width, height]";
SendImage::usage = "SendImage[image]";
GetImage::usage = "GetImage[]";
BlazedMode::usage = "BlazedMode[row, col, ...]";
PixelChooser::usage = "PixelChoose[image_,x_,y_,scale_,hbs_]";
ManipulatePixelChooser::usage = "ManipulatePixelChooser[image_]";
itorc::usage = "itorc[i, rows, cols]";
rctoi::usage = "rctoi[row, col, rows, cols]";
AddPhaseBlock::usage = "AddPhaseBlock[row, col, image, offset]";
PlotAberrationAll::usage = "PlotAberrationAll[phases, table, step]";
GenerateMask::usage = "GenerateMask[flatPhaseMask]";
AddMask::usage = "AddMask[img1, img2]";


(* ::Input::Initialization:: *)
Begin["Private`"];


(* ::Section:: *)
(*Link Management*)


(* ::Input::Initialization:: *)
(* Attempt to connect to WSTP listening under name 'ueyen' 
Example: link named 'ueye1' should be waiting for connection if n is 1.
Function Aborts if activation does not occur within 5 seconds. 
*)
EstablishLink[n_]:=Module[{},
	link = LinkConnect["ueye"<>ToString[n]];
	TimeConstrained[LinkActivate[link],5];
];


(* ::Input::Initialization:: *)
(* Closes all links with names that start with ueye*.
Note: ALWAYS CLOSE LINK AFTER ONE SIDE CLOSES THE LINK/AND/OR CRASHES *)
CloseLink[]:= Module[{},
	LinkClose[#]&/@Links["ueye*"];
	Links["ueye*"];
];



(* ::Input::Initialization:: *)
(* Set some parameter in the Camera, only exposure and gain supported atm *)
SetCameraProperty::notsupp = "Property `1` not supported";
SetCameraProperty[name_, value_]:=Module[{},
	If[!MemberQ[{"exposure", "gain"}, name],
	Message[SetProperty::notsupp, name],
	Check[
		LinkWrite[link, ReturnExpressionPacket[{name, value}]],
		Abort[]]
	];
];



(* ::Input::Initialization:: *)
(* Crop the image before sending to Mathematica - Mathematica lags real hard if image is big  *)
SetImageDimensions::notsupp = "Property `1` not supported";
SetImageDimensions[width_, height_,OptionsPattern[{TopLeft-> {0,0}}]]:=Module[{x,y,w,h},
	x =IntegerPart[OptionValue[TopLeft][[1]]];
	y =IntegerPart[OptionValue[TopLeft][[2]]];
	w = IntegerPart[width];
	h = IntegerPart[height];

	Check[
		LinkWrite[link, ReturnExpressionPacket[{"imgcrop", x, y, w, h}]],
		Abort[]
	]
];



(* ::Input::Initialization:: *)
(* Send an Image to be displayed on the screen, aborts if something goes wrong *)
SendImage::dim = "must be of pixel 2d array with 1 or 3 channels";
SendImage[image_]:=Module[{bytes,n,dim},
	If[ImageQ[image],
		bytes = ImageData[image, "Byte"],
		bytes = image;
	];

	dim = Dimensions@bytes;
	n = Length@dim;
	(*If[n\[Equal]2,
 bytes= ArrayReshape[bytes, {dim[[1]],dim[[2]],1}],
bytes=bytes[[;;,;;,1;;3]]
];*)

	If[n == 2 || n == 3,
		Check[
			LinkWrite[link, ReturnExpressionPacket[{"send", bytes}]];
			Pause[0.5] (* pause for a litle bit, immediately reading when other program isn't ready seems to let all hell break lose and kernel needs to be restarted *),
			Abort[]
		],
		Message[SendImage::dim]
	];
];



(* ::Input::Initialization:: *)
(* Attempts to get the image from the Camera, aborts if an error occurs while reading from link.
If no data is avaiable withint TimeOut, returns None.
*)
GetImage::writetimeout="Timeout when trying to write to link";
GetImage::readtimeout="Timeout when trying to read from link";
GetImage[OptionsPattern[{TimeOut-> 5}]]:=Module[{rt=None},
	Check[
		(*no clue by TimeConstrained doesn't work here... *)
		TimeConstrained[
			LinkWrite[link,ReturnExpressionPacket[{"get"}]],
			OptionValue[TimeOut],
			Message[GetImage::writetimeout]
		],
		Abort[]
	];


	If[LinkReadyQ[link,OptionValue[TimeOut]],
		rt=Check[
			TimeConstrained[LinkRead[link],OptionValue[TimeOut],Message[GetImage::readtimeout]],
			 Abort[]];
		bytes=rt;
		(*PrintTemporary["converting "<>ToString[Dimensions@rt]<>" image ..."];*)
		Image[rt, "Byte"],
		None
	]
];


(* ::Section:: *)
(*Phase Correction*)


(* ::Input::Initialization:: *)
(*blaze 'blocks' of the image*)
BlazedMode[rowt_, colt_, offset_,
	OptionsPattern[
		{
		Width-> 500, 
		Height-> 500,
		Blocks-> {5, 5},
		BlazeAngle-> 45 Degree,
		ReferenceBlock -> "Center",
		UseImage-> None,
		Frequency-> 1/10
	}]]:=
Check[
Module[
	{img, w=OptionValue[Width], h=OptionValue[Height],xr,yr,
nw, nh, angle=OptionValue[BlazeAngle],bw,bh, rb=OptionValue[ReferenceBlock],rxi, ryi,rxr,ryr,rr,cr,frq=OptionValue@Frequency},

If[OptionValue@UseImage ===None,
		img = ConstantArray[0, {h, w}],
		img=If[ImageQ@OptionValue@UseImage,ImageData@OptionValue@UseImage,
		OptionValue@UseImage];
		{h,w}=Dimensions@img;
	];

	If[ListQ@OptionValue@Blocks,
		{nw,nh}=OptionValue@Blocks,
		nw = OptionValue@Blocks;
		nh=nw;
	];

	{bw,bh}=Floor/@{w/nw, h/nh};
	xr[i_]:= (bw*i+1);;(If[i==nw-1,w,bw*(i+1)]);
	yr[i_]:= (bh*i+1);;(If[i==nh-1,h,bh*(i+1)]);

	If[rb =!= None,
		If[!ListQ@rb,
			{ryi,rxi}=Floor/@({nh-1,nw-1}/2),
			{ryi,rxi}=rb;
		];
		rxr=xr[rxi];
		ryr=yr[ryi];
		img[[ryr,rxr]]=LG`BlazeImage[img[[ryr,rxr]],angle,Raw-> True, Frequency-> frq, PhaseOffset-> 0];
	];

	{rr,cr}={yr@rowt,xr@colt};
	img[[rr,cr]]=LG`BlazeImage[img[[rr,cr]],angle,Raw-> True, Frequency-> frq, PhaseOffset-> offset];
	Image[img]
],
	Abort[]
];



(* ::Input::Initialization:: *)
(*to pick a pixel as the intensity probe*)
pixelpos={1,1};
PixelChooser[image_,x_,y_,scale_,hbs_]:=
Check[
Module[
{i,bx, by,bxs,bys,w,h,sx,ex,sy,ey,data,subimg,subimgs,offsetxs,border,offsetys,offsetxe,offsetye,scaledsubimg,smallsubimg,ch=0},
	data=ImageData@image;
	{h,w}=(Dimensions@data)[[;;2]];
	If[Length@Dimensions@data> 2, ch=(Dimensions@data)[[3]]];

	pixelpos={h+1-Min[Max[Floor[y]+1,1],h],Min[Max[Floor[x]+1,1],w]};
	{by,bx}=pixelpos-hbs;


	subimgs=scale*(2*hbs+1);
	subimg=If[ch > 0, ConstantArray[0,{subimgs,subimgs, ch}], ConstantArray[0,{subimgs,subimgs}]];

	{sx,sy}={Max[bx,1],Max[by,1]};
	{ex,ey}={Min[bx+2*hbs,w], Min[by+2*hbs,h]};

	smallsubimg=ImageTake[image,{sy,ey},{sx,ex}];
	scaledsubimg=ImageResize[smallsubimg,{Scaled[scale],Scaled[scale]},Resampling-> "Constant"];
	{offsetxs,offsetys}={If[bx < 1, scale*(-bx+1)+1, 1],If[by< 1, scale*(-by+1)+1, 1]};
	{offsetxe,offsetye}={
			If[bx+2*hbs >w,subimgs+scale*( w-(bx+2*hbs )), subimgs],
			If[by+2*hbs>h, subimgs+scale*(h-(by+2*hbs)), subimgs]
		};

	subimg[[offsetys;;offsetye,offsetxs;;offsetxe,;;]]=ImageData@scaledsubimg;

	border=scale*(hbs+1);


	Row[
		{
		Show[image, ImageSize-> UpTo[500]],
		Spacer[10],
		Show[
			Image@subimg,
	Graphics[
	{Red,Line[{{border,border},{border+scale,border}, {border+scale,border+scale},{border,border+scale}, {border,border}}-scale]}]
	]}
	]
],
Abort[]];




(* ::Input::Initialization:: *)
ManipulatePixelChooser[image_]:= Module[
	{},
	pixelpos={1,1};
	Print["Chosen pixel (row, col): ",Dynamic[pixelpos]];
	Manipulate[
		PixelChooser[image,p[[1]],p[[2]],7,10 ],
		{{p,{0,0}},  Locator}
	]
];




(* ::Section:: *)
(*Import/Parse Data*)


(* ::Input::Initialization:: *)
(* row-wise index to (row, col) *)
itorc[i_,rows_:18, cols_:32]:={Floor[(i-1)/cols]+1, Mod[(i-1), cols]+1}; 
(* (row,col) to row wise index*)
rctoi[row_,col_,row_:18,cols_:32]:=row*cols + col + 1; 



(* ::Input::Initialization:: *)
(* returns 'image' with (colt, rowt) block values shited by 'offset' *)
AddPhaseBlock[rowt_, colt_, image_,offset_,
OptionsPattern[
{
Blocks-> {32, 18},
Raw-> False
}]]:=
Check[
 Module[
	{img, w,h,xr,yr,raw=OptionValue[Raw],
	 nw, nh,bw,bh,rxi, ryi,rxr,ryr,rr,cr},
	raw=!ImageQ@image;
	img=If[raw,image,ImageData@image];
	{h,w}=(Dimensions@img)[[;;2]];

	If[ListQ@OptionValue@Blocks,
		{nw,nh}=OptionValue@Blocks,
		nw = OptionValue@Blocks;
		nh=nw;
	];

	{bw,bh}=Floor/@{w/nw, h/nh};
	xr[i_]:= (bw*i+1);;(If[i==nw-1,w,bw*(i+1)]);
	yr[i_]:= (bh*i+1);;(If[i==nh-1,h,bh*(i+1)]);

	{rr,cr}={yr@rowt,xr@colt};
	img[[rr,cr]]= Mod[img[[rr,cr]]+offset,1.0];

	If[raw, img,Image[img]]],
	Abort[]
];



(* ::Input::Initialization:: *)
PlotAberrationAll[phases_, table_,step_:0.05,OptionsPattern[{Columns-> 3}]]:=Module[{maxi,plots,p,n},
	PrintTemporary["Generating \[Phi] = ", Dynamic[p]];
	plots=Table[
	maxi=table[[i,2]];
	p = i*step;
	ListLinePlot[phases[[i]], 
		PlotLabel->ToString[itorc[i]],
		AxesLabel-> {"Phase ([0,1])", "Grayscale "},
		Epilog-> {
			Red,
			Line[{{maxi*step, 0}, {maxi*step, table[[i,maxi+3]]}}]
		}], 
		{i, 1, Length@phases,1}
	];
	n = Length@phases;
	Grid[ArrayReshape[plots, {Floor[n/OptionValue[Columns]],OptionValue[Columns]}]]
];



(* ::Input::Initialization:: *)
GenerateMask[flatPhaseMask_,OptionsPattern[{Width-> 1920, Height-> 1080, Raw-> False}]]:=Module[
	{w=OptionValue[Width], h=OptionValue[Height],imgv,prog,i,pos},
	imgv=ConstantArray[0,{h,w}];
	PrintTemporary["Generating ", Dynamic[prog]];
	For[i=1, i <= Length[flatPhaseMask], i+=1,
		pos=itorc[i];
		prog=ToString[i] <>" "<>ToString[pos];
		imgv=AddPhaseBlock[pos[[1]]-1,pos[[2]]-1,imgv, flatPhaseMask[[i]], Raw-> True];
	];

	If[OptionValue[Raw], imgv, Image[imgv]]
];



(* ::Input::Initialization:: *)
AddMask[img1_,img2_, OptionsPattern[{Raw-> False}]]:=Module[{i1,i2,i3},
	If[ImageQ@img1, i1=ImageData[img1], i1=img1];
	If[ImageQ@img2, i2=ImageData[img2], i2=img2];
	If[Length@Dimensions@i1!= 2,
		i1=ImageData@ColorConvert[If[ImageQ@img1, img1, Image[img1]], "Grayscale"];
	];
	If[Length@Dimensions@i2!= 2,
		i2=ImageData@ColorConvert[If[ImageQ@img2, img2, Image[img2]], "Grayscale"];
	];

	i3=Mod[i1+i2,1.0];

	If[OptionValue[Raw], i3, Image[i3]]
];


(* ::Section:: *)
(*Footer*)


(* ::Input::Initialization:: *)
End[];


(* ::Input::Initialization:: *)
EndPackage[];
