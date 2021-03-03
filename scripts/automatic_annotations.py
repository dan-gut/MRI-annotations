import struct
import numpy as np
from math import isnan
from skimage.segmentation import flood
from itertools import chain, combinations


def read_binary_data(filepath, img_size, slices_no, nbytes):
    if nbytes == 4:
        img = np.zeros((img_size, img_size, slices_no), dtype=np.float32)
    elif nbytes == 2:
        img = np.zeros((img_size, img_size, slices_no), dtype=np.uint16)
    elif nbytes == 1:
        img = np.zeros((img_size, img_size, slices_no), dtype=np.uint8)
    else:
        assert AttributeError('Wrong number of bytes per voxel')
        return

    f = open(filepath, "rb")
    for i in range(slices_no):
        if nbytes == 4:
            form = f'>{img_size**2}f'
            num = struct.unpack(form, f.read(4 * img_size**2))
            img[:, :, i] = np.asarray(num, dtype=np.float32).reshape((img_size, img_size))
        else:
            for j in range(img_size):
                for k in range(img_size):
                    byte = f.read(nbytes)
                    if nbytes == 2:
                        val = 256 * byte[0] + byte[1]  # big endian
                        # val = 256 * byte[1] + byte[0]  # little endian
                        img[j, k, i] = val
                    else:
                        val = byte[0]
                        img[j, k, i] = val
    f.close()
    return img


def write_binary_data(filepath, img):
    f = open(filepath, "wb")
    for i in range(img.shape[2]):
        for j in range(img.shape[0]):
            for k in range(img.shape[0]):
                f.write(img[j, k, i])
    f.close()


def powerset(iterable):
    """powerset([1,2,3]) --> () (1,) (2,) (3,) (1,2) (1,3) (2,3) (1,2,3)"""
    s = list(iterable)
    return chain.from_iterable(combinations(s, r) for r in range(len(s)+1))


def iou(arrayA, arrayB):
    intersection = np.sum(np.logical_and(arrayA, arrayB))
    union = np.sum(arrayA + arrayB)
    return intersection/union


def dice(arrayA, arrayB):
    intersection = np.sum(np.logical_and(arrayA, arrayB))
    union = np.sum(arrayA) + np.sum(arrayB)
    return 2*intersection/union


def compare(original_filepath, new_filepath, img_size, slices_no, original_lesion_val=255, new_lesion_val=255):
    original_lesions = read_binary_data(original_filepath, img_size, slices_no, 1) == original_lesion_val
    new_lesions = read_binary_data(new_filepath, img_size, slices_no, 1) == new_lesion_val

    iou_val = iou(original_lesions, new_lesions)
    dice_val = dice(original_lesions, new_lesions)

    return iou_val, dice_val


def compare_by_slice(original_filepath, new_filepath, img_size, slices_no, original_lesion_val=255, new_lesion_val=255):
    original_lesions = read_binary_data(original_filepath, img_size, slices_no, 1) == original_lesion_val
    new_lesions = read_binary_data(new_filepath, img_size, slices_no, 1) == new_lesion_val

    iou_vals = []
    dice_vals = []

    for i in range(slices_no):
        iou_vals.append(iou(original_lesions[:, :, i], new_lesions[:, :, i]))
        dice_vals.append(dice(original_lesions[:, :, i], new_lesions[:, :, i]))

    return np.nanmean(iou_vals), np.nanmean(dice_vals)


def make_annotations(lesions_filepath, merged_filepath, output_path, img_size, slices_no,
                     down_threshold=0.1, up_threshold=0.9):

    print(f'loading {lesions_filepath}')
    lesions = read_binary_data(lesions_filepath, img_size, slices_no, 1)
    lesions_bool = lesions == 255
    lesions_merged = np.zeros((img_size, img_size, slices_no), dtype=np.uint8)

    print(f'loading {merged_filepath}')
    merged = read_binary_data(merged_filepath, img_size, slices_no, 2)

    for i in range(slices_no):
        print(f'*** processing slice #{i}')
        ind = np.where(lesions_bool[:, :, i])
        lesions_coordinates = list(zip(ind[0], ind[1]))
        intersect_mask = np.zeros((400, 400), dtype=bool)
        add_mask_list = []
        for p in lesions_coordinates:
            mask = flood(merged[:, :, i], p)
            intersection_level = np.sum(np.logical_and(mask, lesions_bool[:, :, i]))/np.sum(mask)
            if intersection_level > up_threshold:
                intersect_mask += mask
            elif intersection_level > down_threshold and not any(np.array_equal(mask, m) for m in add_mask_list):
                add_mask_list.append(mask)

        # plt.imshow(intersect_mask, cmap='gray')
        # plt.show()

        mask_powerset = powerset(add_mask_list)
        # print(f'powerset size = {len(list(mask_powerset))}')

        best_IoU = 0
        best_mask = np.zeros((400, 400), dtype=bool)
        for mask_set in mask_powerset:
            sum_mask = np.copy(intersect_mask)
            for mask in mask_set:
                sum_mask += mask

            curr_IoU = iou(sum_mask, lesions_bool[:, :, i])
            if best_IoU < curr_IoU:
                best_mask = np.copy(sum_mask)
                best_IoU = curr_IoU

        # plt.imshow(best_mask, cmap='gray')
        # plt.show()

        lesions_merged[:, :, i] = best_mask * 255

    print(f'saving {output_path}')
    write_binary_data(output_path, lesions_merged)


if __name__ == '__main__':
    from time import time
    start_time = time()

    similarity = {}

    import os

    LESION_DIR = '../../../SpA_Data/GroundTruth_MajorityVoting'
    MERGED_DIR = '../../../SpA_Data/Merged/Q25'
    MERGED_LES_DIR = '../../../SpA_Data/Merged/Lesions(Q25)_2'

    for filename in os.listdir(LESION_DIR):
        if filename.endswith(".raw"):
            _, case_no, img_size, _, slices_no, _, _ = filename.split('_')

            lesions_filepath = LESION_DIR + '/' + filename
            merged_filepath = MERGED_DIR + '/' + f'OS_{case_no}_{img_size}_{img_size}_{slices_no}_1_.raw'
            merged_les_filepath = MERGED_LES_DIR + '/' + f'ML_{case_no}_{img_size}_{img_size}_{slices_no}_1_.raw'

            make_annotations(lesions_filepath, merged_filepath, merged_les_filepath, int(img_size), int(slices_no))

            similarity[case_no] = compare_by_slice(lesions_filepath, merged_les_filepath, int(img_size), int(slices_no))
            print(f'#{case_no}: {similarity[case_no]}')
        else:
            continue

    print(time()-start_time)

    similarity = {k: v for k, v in similarity.items() if not isnan(v[0])}

    f = open(MERGED_LES_DIR + '/similarity_by_slice.txt', "w")
    f.write(str(similarity))
    f.close()
