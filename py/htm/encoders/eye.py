# Written by David McDougall, 2018

"""
From Wikipedia "Retina": Although there are more than 130 million
retinal receptors, there are only approximately 1.2 million fibres
(axons) in the optic nerve; a large amount of pre-processing is
performed within the retina. The fovea produces the most accurate
information. Despite occupying about 0.01% of the visual field (less
than 2 of visual angle), about 10% of axons in the optic nerve are
devoted to the fovea.

For more information on visual processing on retina, see 
https://foundationsofvision.stanford.edu/chapter-5-the-retinal-representation/#visualinformation


Fun Fact 1: The human optic nerve has 800,000 ~ 1,700,000 nerve fibers.
Fun Fact 2: The human eye can distiguish between 10 million different colors.
~Sources: Wikipedia. 
"""

import math
import random

import numpy as np
import cv2 # pip install opencv-contrib-python
from PIL import Image # pip Pillow

from htm.bindings.sdr import SDR


class ChannelEncoder:
    """
    This assigns a random range to each bit of the output SDR.  Each bit becomes
    active if its corresponding input falls in its range.  By using random
    ranges, each bit represents a different thing even if it mostly overlaps
    with other comparable bits.  This way redundant bits add meaning.
    """
    def __init__(self, input_shape, num_samples, sparsity,
        dtype       = np.float64,
        drange      = range(0,1),
        wrap        = False):
        """
        Argument input_shape is tuple of dimensions for each input frame.

        Argument num_samples is number of bits in the output SDR which will
                 represent each input number, this is the added data depth.

        Argument sparsity is fraction of output which on average will be active.
                 This is also the fraction of the input spaces which (on 
                 average) each bin covers.

        Argument dtype is numpy data type of channel.

        Argument drange is a range object or a pair of values representing the 
                 range of possible channel values.

        Argument wrap ... default is False.
                 This supports modular input spaces and ranges which wrap
                 around. It does this by rotating the inputs by a constant
                 random amount which hides where the discontinuity in ranges is.
                 No ranges actually wrap around the input space.
        """
        self.input_shape  = tuple(input_shape)
        self.num_samples  = int(round(num_samples))
        self.sparsity     = sparsity
        self.output_shape = self.input_shape + (self.num_samples,)
        self.dtype        = dtype
        self.drange       = drange
        self.len_drange   = max(drange) - min(drange)
        self.wrap         = bool(wrap)
        if self.wrap:
            # Each bit responds to a range of input values, length of range is 2*Radius.
            radius        = self.len_drange * self.sparsity / 2
            self.offsets  = np.random.uniform(0, self.len_drange, self.input_shape)
            self.offsets  = np.array(self.offsets, dtype=self.dtype)
            # If wrapping is enabled then don't generate ranges which will be
            # truncated near the edges.
            centers = np.random.uniform(min(self.drange) + radius,
                                        max(self.drange) - radius,
                                        size=self.output_shape)
        else:
            # Buckets near the edges of the datarange are OK.  They will not
            # respond to a full range of input values but are needed to
            # represent the bits at the edges of the data range.
            #
            # Expand the data range and create bits which will encode for the
            # edges of the datarange to ensure a resonable sparsity at the
            # extremes of the data ragne.  Increase the size of all of the
            # buckets to accomidate for the extra area being represented.
            M = .95 # Maximum fraction of bucket-size outside of the true data range to allocate buckets.
            len_pad_drange = self.len_drange / (1 - M * self.sparsity)
            extra_space    = (len_pad_drange - self.len_drange) / 2
            pad_drange     = (min(self.drange) - extra_space, max(self.drange) + extra_space)
            radius         = len_pad_drange * self.sparsity / 2
            centers = np.random.uniform(min(pad_drange),
                                        max(pad_drange),
                                        size=self.output_shape)
        # Make the lower and upper bounds of the ranges.
        low  = np.clip(centers - radius, min(self.drange), max(self.drange))
        high = np.clip(centers + radius, min(self.drange), max(self.drange))
        self.low  = np.array(low, dtype=self.dtype)
        self.high = np.array(high, dtype=self.dtype)

    def encode(self, img):
        """Returns a dense boolean np.ndarray."""
        assert(img.shape == self.input_shape)
        assert(img.dtype == self.dtype)
        if self.wrap:
            img += self.offsets
            # Technically this should subtract min(drange) before doing modulus
            # but the results should also be indistinguishable B/C of the random
            # offsets.  Min(drange) effectively becomes part of the offset.
            img %= self.len_drange
            img += min(self.drange)
        img = img.reshape(img.shape + (1,))
        return np.logical_and(self.low <= img, img <= self.high)


class Eye:
    """
    Optic sensor with central fovae.

    Attribute output_sdr ... retina's output
    Attribute roi ... The most recent view, kept as a attribute.
    Attribute parvo ... SDR with parvocellular pathway (color)
    Attribute magno ... SDR with magnocellular pathway (movement) 

    The following three attributes control where the eye is looking within
    the image.  They are Read/Writable.
    Attribute position     (X, Y) coords of eye center within image
    Attribute orientation  ... units are radians TODO of what? sensor/image
    Attribute scale        ... TODO
    """
    def __init__(self,
        output_diameter   = 200,
        sparsity          = .2,
        mode              = "both",
        color             = False,
        plot              = False,):
        """
        Argument output_diameter is size of output ... output is a 
            field of view (image) with circular shape. Default 200
        Argument sparsity is fraction of bits in eye.output_sdr which are 
            active, on average. Default 0.2 (=20%)
        Argument mode: one of "parvo", "magno", "both". Which retinal cells 
            to emulate. Default "both".
        Argument color: True/False. Emulate color vision, or only B/W?
            Default True.
        Argument plot: True/False. Whether to display plots, default False.
        """
        self.output_diameter   = output_diameter
        # Argument resolution_factor is used to expand the sensor array so that
        # the fovea has adequate resolution.  After log-polar transform image
        # is reduced by this factor back to the output_diameter.
        self.resolution_factor = 3
        self.retina_diameter   = int(self.resolution_factor * output_diameter)
        # Argument fovea_scale  ... represents "zoom" aka distance from the object/image.
        self.fovea_scale       = 0.177
        assert(output_diameter // 2 * 2 == output_diameter) # Diameter must be an even number.
        assert(self.retina_diameter // 2 * 2 == self.retina_diameter) # (Resolution Factor X Diameter) must be an even number.
        assert(mode in ["magno", "parvo", "both"])
        assert(color is False or color is True)
        # color, or B/W vision
        self.color = color
        assert(plot is False or plot is True)
        # plot images?
        self.plot = plot

        self.output_sdr = SDR((output_diameter, output_diameter, 2,))

        self.retina = cv2.bioinspired.Retina_create(
            inputSize            = (self.retina_diameter, self.retina_diameter),
            colorMode            = color,
            colorSamplingMethod  = cv2.bioinspired.RETINA_COLOR_BAYER,)

        print(self.retina.printSetup())
        print()

        if mode == "both" or mode == "parvo":
          dims = (output_diameter, output_diameter)
          if color is True: 
            dims = (output_diameter, output_diameter, 3,)

          self.parvo_enc = ChannelEncoder(
                            input_shape = dims,
                            num_samples = 1, 
                            sparsity = sparsity * (1/3.), #biologically, parvocellular pathway is only 33% of the magnocellular (in terms of cells)
                            dtype=np.uint8, drange=[0, 255,])
        else:
          self.parvo_enc = None

        if mode == "both" or mode == "magno":
          self.magno_enc = ChannelEncoder(
                            input_shape = (output_diameter, output_diameter),
                            num_samples = 1, 
                            sparsity = sparsity,
                            dtype=np.uint8, drange=[0, 255],)
        else:
          self.magno_enc = None

        # the current input image
        self.image = None


    def new_image(self, image):
        """
        Argument image ...
            If String, will load image from file path.
            If numpy.ndarray, will attempt to cast to correct data type and
                dimensions.
        """
        # Load image if needed.
        if isinstance(image, str):
            self.image = np.array(Image.open(image), copy=False)
        else:
            self.image = image
        # Get the image into the right format.
        assert(isinstance(self.image, np.ndarray))
        if self.image.dtype != np.uint8:
            raise TypeError('Image "%s" dtype is not unsigned 8 bit integer, image.dtype is %s.'%(
                self.image.dtype))
        # Ensure there are three color channels.
        if len(self.image.shape) == 2 or self.image.shape[2] == 1:
            self.image = np.dstack([self.image] * 3)
        # Drop the alpha channel if present.
        elif self.image.shape[2] == 4:
            self.image = self.image[:,:,:3]
        # Sanity checks.
        assert(len(self.image.shape) == 3)
        assert(self.image.shape[2] == 3) # Color images only.
        self.reset()
        self.center_view()

    def center_view(self):
        """Center the view over the image"""
        self.orientation = 0
        self.position    = (self.image.shape[0]/2., self.image.shape[1]/2.)
        self.scale       = np.min(np.divide(self.image.shape[:2], self.retina_diameter))

    def randomize_view(self, scale_range=None):
        """Set the eye's view point to a random location"""
        if scale_range is None:
            scale_range = [2, min(self.image.shape[:2]) / self.retina_diameter]
        self.orientation = random.uniform(0, 2 * math.pi)
        self.scale       = random.uniform(min(scale_range), max(scale_range))
        roi_radius       = self.scale * self.retina_diameter / 2
        self.position    = [random.uniform(roi_radius, dim - roi_radius)
                                 for dim in self.image.shape[:2]]

    def _crop_roi(self):
        """
        Crop to Region Of Interest (ROI) which contains the whole field of view.
        Note that the size of the ROI is (eye.output_diameter *
        eye.resolution_factor).

        Arguments: eye.scale, eye.position, eye.image

        Returns RGB image.
        """
        assert(self.image is not None)

        r     = int(round(self.scale * self.retina_diameter / 2))
        x, y  = self.position
        x     = int(round(x))
        y     = int(round(y))
        x_max, y_max, color_depth = self.image.shape
        # Find the boundary of the ROI and slice out the image.
        x_low  = max(0, x-r)
        x_high = min(x_max, x+r)
        y_low  = max(0, y-r)
        y_high = min(y_max, y+r)
        image_slice = self.image[x_low : x_high, y_low : y_high]
        # Make the ROI and insert the image into it.
        roi = np.zeros((2*r, 2*r, 3,), dtype=np.uint8)
        if x-r < 0:
            x_offset = abs(x-r)
        else:
            x_offset = 0
        if y-r < 0:
            y_offset = abs(y-r)
        else:
            y_offset = 0
        x_shape, y_shape, color_depth = image_slice.shape
        roi[x_offset:x_offset+x_shape, y_offset:y_offset+y_shape] = image_slice
        # Rescale the ROI to remove the scaling effect.
        roi = np.array(Image.fromarray(roi).resize( (self.retina_diameter, self.retina_diameter)))
        return roi

    def compute(self):
        self.roi = self._crop_roi()

        # Retina image transforms (Parvo & Magnocellular).
        self.retina.run(self.roi)
        if self.parvo_enc is not None:
          parvo = self.retina.getParvo()
        if self.magno_enc is not None:
          magno = self.retina.getMagno()

        # Log Polar Transform.
        center = self.retina_diameter / 2
        M      = self.retina_diameter * self.fovea_scale
        if self.parvo_enc is not None:
          parvo = cv2.logPolar(parvo,
            center = (center, center),
            M      = M,
            flags  = cv2.WARP_FILL_OUTLIERS)
          parvo = np.array(Image.fromarray(parvo).resize( (self.output_diameter, self.output_diameter)))

        if self.magno_enc is not None:
          magno = cv2.logPolar(magno,
            center = (center, center),
            M      = M,
            flags  = cv2.WARP_FILL_OUTLIERS)
          magno = np.array(Image.fromarray(magno).resize( (self.output_diameter, self.output_diameter)))

        # Apply rotation by rolling the images around axis 1.
        rotation = self.output_diameter * self.orientation / (2 * math.pi)
        rotation = int(round(rotation))
        if self.parvo_enc is not None:
          self.parvo = np.roll(parvo, rotation, axis=0)
        if self.magno_enc is not None:
          self.magno = np.roll(magno, rotation, axis=0)

        # Encode images into SDRs.
        p = []
        m = []
        if self.parvo_enc is not None:
          p   = self.parvo_enc.encode(self.parvo)
          if self.color:
            pr, pg, pb = np.dsplit(p, 3)
            p   = np.logical_and(np.logical_and(pr, pg), pb)
          p   = np.expand_dims(np.squeeze(p), axis=2)
          sdr = p
        if self.magno_enc is not None:
          m   = self.magno_enc.encode(self.magno)
          sdr = m
        if self.magno_enc is not None and self.parvo_enc is not None:
          sdr = np.concatenate([p, m], axis=2)

        self.output_sdr.dense = sdr
        return self.output_sdr


    def make_roi_pretty(self, roi=None):
        """
        Makes the eye's view look more presentable.
        - Adds a black circular boarder to mask out areas which the eye can't see
          Note that this boarder is actually a bit too far out, playing with
          eye.fovea_scale can hide areas which this ROI image will show.
        - Adds 5 dots to the center of the image to show where the fovea is.

        Returns an RGB image.
        """
        if roi is None:
            roi = self.roi

        # Show the ROI, first rotate it like the eye is rotated.
        angle = self.orientation * 360 / (2 * math.pi)
        roi = self.roi[:,:,::-1]
        rows, cols, color_depth = roi.shape
        M   = cv2.getRotationMatrix2D((cols / 2, rows / 2), angle, 1)
        roi = cv2.warpAffine(roi, M, (cols,rows))

        # Mask out areas the eye can't see by drawing a circle boarder.
        center = int(roi.shape[0] / 2)
        circle_mask = np.zeros(roi.shape, dtype=np.uint8)
        cv2.circle(circle_mask, (center, center), center, thickness = -1, color=(255,255,255))
        roi = np.minimum(roi, circle_mask)

        # Invert 5 pixels in the center to show where the fovea is located.
        roi[center, center]     = np.full(3, 255) - roi[center, center]
        roi[center+2, center+2] = np.full(3, 255) - roi[center+2, center+2]
        roi[center-2, center+2] = np.full(3, 255) - roi[center-2, center+2]
        roi[center-2, center-2] = np.full(3, 255) - roi[center-2, center-2]
        roi[center+2, center-2] = np.full(3, 255) - roi[center+2, center-2]
        return roi

    def show_view(self, window_name='Eye', delay=1):
        """plot the retina's output SDRs. 
        Argument delay: ms to wait between saccadic movements.
          Default 1ms.
        """
        if self.plot is False:
          return

        roi = self.make_roi_pretty()
        cv2.imshow('Region Of Interest', roi)
        if self.color:
          cv2.imshow('Parvocellular', self.parvo[:,:,::-1])
        else:
          cv2.imshow('Parvocellular', self.parvo)
        cv2.imshow('Magnocellular', self.magno)
        cv2.waitKey(delay)

    def input_space_sample_points(self, npoints):
        """
        Returns a sampling of coordinates which the eye is currently looking at.
        Use the result to determine the actual label of the image in the area
        where the eye is looking.
        """
        # Find the retina's radius in the image.
        r = int(round(self.scale * self.retina_diameter / 2))
        # Shrink the retina's radius so that sample points are nearer the fovea.
        # Also shrink radius B/C this does not account for the diagonal
        # distance, just the manhattan distance.
        r = r * 2/3
        # Generate points.
        coords = np.random.random_integers(-r, r, size=(npoints, 2))
        # Add this position offset.
        coords += np.array(np.rint(self.position), dtype=np.int).reshape(1, 2)
        return coords

    def small_random_movement(self): #TODO pass as custom fn
        max_change_angle = (2*3.14159) / 500
        self.position = (
            self.position[0] + random.gauss(1, .75),
            self.position[1] + random.gauss(1, .75),)
        self.orientation += random.uniform(-max_change_angle, max_change_angle)
        self.scale = 1

    def reset(self):
        self.retina.clearBuffers()



def _get_images(path):
    """ Returns list of all image files found under the given file path. """
    image_extensions = [
        '.bmp',
        '.dib',
        '.png',
        '.jpg',
        '.jpeg',
        '.jpe',
        '.tif',
        '.tiff',
    ]
    images = []
    import os
    if os.path.isfile(path):
        basename, ext = os.path.splitext(path)
        if ext.lower() in image_extensions:
            images.append( path )
    else:
        for dirpath, dirnames, filenames in os.walk(path):
            for fn in filenames:
                basename, ext = os.path.splitext(fn)
                if ext.lower() in image_extensions:
                    images.append( os.path.join(dirpath, fn) )
    return images

if __name__ == '__main__':
    import argparse
    args = argparse.ArgumentParser()
    args.add_argument('IMAGE', type=str)
    args   = args.parse_args()
    images = _get_images( args.IMAGE )
    random.shuffle(images)
    if not images:
        print('No images found at file path "%s"!'%args.IMAGE)
    else:
        eye = Eye(plot=True)
        for img_path in images:
            eye.reset()
            print("Loading image %s"%img_path)
            eye.new_image(img_path)
            eye.scale = 1
            for i in range(10):
                sdr = eye.compute()
                eye.show_view()
                eye.small_random_movement()
        print("All images seen.")
